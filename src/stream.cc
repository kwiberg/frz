/*
  Copyright 2021 Karl Wiberg

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "stream.hh"

#include <absl/base/thread_annotations.h>
#include <absl/synchronization/mutex.h>
#include <algorithm>
#include <deque>
#include <optional>

#include "assert.hh"
#include "worker.hh"

namespace frz {
namespace {

// A very simple Streamer that will sequentially get bytes from the source,
// feed them to the sink, and repeat until the stream ends.
class SingleThreadedStreamer final : public Streamer {
  public:
    SingleThreadedStreamer(CreateSingleThreadedStreamerArgs args)
        : buffer_(std::make_unique<std::byte[]>(args.buffer_size)),
          buffer_size_(args.buffer_size) {}

    void Stream(StreamSource& source, StreamSink& sink,
                std::function<void(int num_bytes)> progress) override {
        const std::span<std::byte> buffer(buffer_.get(), buffer_size_);
        while (true) {
            const auto result = source.GetBytes(buffer);
            if (auto* bc = std::get_if<StreamSource::BytesCopied>(&result)) {
                sink.AddBytes(buffer.subspan(0, bc->num_bytes));
                progress(bc->num_bytes);
            } else if (std::get_if<StreamSource::End>(&result)) {
                break;
            } else {
                FRZ_CHECK(false);
            }
        }
    }

    void ForkedStream(ForkedStreamArgs args) override {
        const std::span<std::byte> buffer(buffer_.get(), buffer_size_);
        while (true) {
            const auto result = args.source.GetBytes(buffer);
            if (auto* bc = std::get_if<StreamSource::BytesCopied>(&result)) {
                args.primary_sink.AddBytes(buffer.subspan(0, bc->num_bytes));
                args.primary_progress(bc->num_bytes);
                args.secondary_sink.AddBytes(buffer.subspan(0, bc->num_bytes));
                args.secondary_progress(bc->num_bytes);
            } else if (std::get_if<StreamSource::End>(&result)) {
                static_cast<void>(args.primary_done());
                break;
            } else {
                FRZ_CHECK(false);
            }
        }
    }

  private:
    const std::unique_ptr<std::byte[]> buffer_;
    const std::size_t buffer_size_;
};

// Move-only object that owns a heap-allocated array of bytes (size fixed at
// construction time), and keeps track of (1) the number of valid bytes and (2)
// whether this buffer contains the last byte of the stream.
//
// After being moved from or default constructed, the object must be assigned a
// new value before it can be read or written.
class StreamBuffer final {
  public:
    // Default constructor. Creates the buffer in an invalid state, just as if
    // it had been moved from.
    StreamBuffer() : data_(nullptr), capacity_(0) { FRZ_ASSERT(!Valid()); }

    explicit StreamBuffer(int size)
        : data_(std::make_unique<std::byte[]>(size)), capacity_(size) {
        FRZ_ASSERT(Valid());
    }

    StreamBuffer(StreamBuffer&&) = default;
    StreamBuffer& operator=(StreamBuffer&&) = default;
    StreamBuffer(const StreamBuffer&) = delete;
    StreamBuffer& operator=(const StreamBuffer&) = delete;

    // Return the span of valid bytes.
    std::span<const std::byte> Read() const {
        FRZ_ASSERT(Valid());
        return std::span(data_.get(), status_.size);
    }

    // Is the last byte in this buffer also the last byte of the whole stream?
    bool End() const {
        FRZ_ASSERT(Valid());
        return status_.end;
    }

    // Return a writable span for the whole buffer. Callers must call
    // .FinishWrite() when they're done writing to it.
    std::span<std::byte> Write() {
        FRZ_ASSERT(Valid());
        return std::span(data_.get(), capacity_);
    }

    // Inform the buffer of how many bytes were written to it, and whether the
    // last byte written is the last byte of the whole stream.
    struct WriteStatus {
        int size;
        bool end;
    };
    void FinishWrite(WriteStatus status) {
        FRZ_ASSERT(Valid());
        FRZ_ASSERT_LE(status.size, capacity_);
        status_ = status;
        FRZ_ASSERT(Valid());
    }

  private:
    bool Valid() const { return data_ && status_.size <= capacity_; }

    std::unique_ptr<std::byte[]> data_;
    int capacity_;
    WriteStatus status_ = {.size = 0, .end = false};
};

// A queue of StreamBuffers. Its methods may be called concurrently with each
// other, but no method may be called concurrently with itself.
class StreamBufferQueue final {
  public:
    // Create a new circular stream buffer that may grow to at most
    // `max_buffers` buffers, each of size `bytes_per_buffer`.
    StreamBufferQueue(int max_buffers, int bytes_per_buffer)
        : bytes_per_buffer_(bytes_per_buffer),
          buffer_allocation_budget_(max_buffers) {}

    // Clear the queue without freeing any memory.
    void Clear() {
        absl::MutexLock ml_unused(&unused_mutex_);
        absl::MutexLock ml_filled(&filled_mutex_);
        for (StreamBuffer& buf : filled_) {
            unused_.push_back(std::move(buf));
        }
        filled_.clear();
    }

    // Get an unused buffer and write to it. Will block if there are no free
    // buffers, and we've reached the limit for how many we may allocate.
    void Enqueue(std::function<void(StreamBuffer& buf)> write_fun) {
        bool success = Enqueue(write_fun, /*may_block=*/true);
        FRZ_ASSERT(success);  // only nonblocking Enqueue() can fail
    }

    // Get an unused buffer and write to it. Will return false without running
    // `write_fun` if there are no free buffers, and we've reached the limit
    // for how many we may allocate. Return true on success.
    bool NonblockingEnqueue(std::function<void(StreamBuffer& buf)> write_fun) {
        return Enqueue(write_fun, /*may_block=*/false);
    }

    // Get the oldest full buffer and read from it. Will block until there is a
    // full buffer available.
    void Dequeue(std::function<void(const StreamBuffer& buf)> read_fun) {
        StreamBuffer buf;

        // Grab the "filled" mutex, blocking until the "filled" queue isn't
        // empty, and then pop the frontost buffer off the queue.
        {
            auto not_blocked = [&] { return !filled_.empty(); };
            absl::MutexLock ml(&filled_mutex_, absl::Condition(&not_blocked));
            FRZ_ASSERT(!filled_.empty());
            buf = std::move(filled_.back());
            filled_.pop_back();
        }

        // Let the caller read from the buffer.
        read_fun(buf);

        // Push the newly unused buffer onto the "unused" stack.
        {
            absl::MutexLock ml(&unused_mutex_);
            unused_.push_back(std::move(buf));
        }
    }

  private:
    // Get an unused buffer and write to it. Will block (if `may_block` is
    // true) or abort without running `write_fun` (if `may_block` is false)
    // if there are no free buffers, and we've reached the limit for how many
    // we may allocate. Return true if all went well, false if we aborted.
    bool Enqueue(std::function<void(StreamBuffer& buf)>& write_fun,
                 bool may_block) {
        StreamBuffer buf;

        if (buffer_allocation_budget_ >= 1) {
            // Grab the "unused" mutex without blocking, and pop the topmost
            // unused buffer off the stack. If the "unused" stack is empty,
            // release the mutex and then allocate a new buffer.
            bool allocate_new = false;
            {
                absl::MutexLock ml(&unused_mutex_);
                if (unused_.empty()) {
                    --buffer_allocation_budget_;
                    allocate_new = true;
                } else {
                    buf = std::move(unused_.back());
                    unused_.pop_back();
                }
            }
            if (allocate_new) {
                buf = StreamBuffer(bytes_per_buffer_);
            }
        } else {
            // Grab the "unused" mutex, blocking until the "unused" stack isn't
            // empty, and then pop the topmost unused buffer off the stack.
            auto not_blocked = [&] { return !may_block || !unused_.empty(); };
            absl::MutexLock ml(&unused_mutex_, absl::Condition(&not_blocked));
            if (unused_.empty()) {
                // No unused buffer was immediately available.
                return false;
            }
            buf = std::move(unused_.back());
            unused_.pop_back();
        }

        // Let the caller fill the buffer.
        write_fun(buf);

        // Add the newly filled buffer to the back of the "filled" queue.
        {
            absl::MutexLock ml(&filled_mutex_);
            filled_.push_back(std::move(buf));
        }

        return true;
    }

    const int bytes_per_buffer_;

    // How many more buffers may we allocate? Not protected by a mutex, because
    // it's only touched by `.Enqueue()`.
    int buffer_allocation_budget_;

    // Unused buffers. A stack, because while we don't care about the data in
    // these buffers, we prefer to reuse memory that is cache hot.
    absl::Mutex unused_mutex_;
    std::vector<StreamBuffer> unused_ ABSL_GUARDED_BY(unused_mutex_);

    // Filled buffers. A queue, because we must stream data in FIFO order.
    // TODO: For better performance, reimplement as a fixed-size circular
    // buffer protected by two `std::counting_semaphore`.
    absl::Mutex filled_mutex_;
    std::deque<StreamBuffer> filled_ ABSL_GUARDED_BY(filled_mutex_);
};

// TODO: Replace with `std::latch` once we have standard library support for
// it.
class Latch final {
  public:
    void CountDown() {
        absl::MutexLock ml(&mutex_);
        FRZ_ASSERT(!unlocked_);
        unlocked_ = true;
    }

    void Wait() {
        auto not_blocked = [&] { return unlocked_; };
        absl::MutexLock ml(&mutex_, absl::Condition(&not_blocked));
    }

  private:
    absl::Mutex mutex_;
    bool unlocked_ ABSL_GUARDED_BY(mutex_) = false;
};

// A Streamer that runs the source in a woker thread and the sink in the
// current thread; this allows them to execute in parallel.
class MultiThreadedStreamer final : public Streamer {
  public:
    MultiThreadedStreamer(CreateMultiThreadedStreamerArgs args)
        : primary_queue_(args.num_buffers, args.bytes_per_buffer),
          secondary_queue_(args.num_buffers_secondary, args.bytes_per_buffer) {}

    void Stream(StreamSource& source, StreamSink& sink,
                std::function<void(int num_bytes)> progress) override {
        primary_queue_.Clear();  // in case an earlier operation was interrupted

        auto source_work = [&] {
            for (bool end = false; !end;) {
                primary_queue_.Enqueue([&](StreamBuffer& buf) {
                    auto result = FillBufferFromStream(source, buf.Write());
                    buf.FinishWrite(
                        {.size = result.num_bytes, .end = result.end});
                    end = result.end;
                });
            }
        };

        auto sink_work = [&] {
            for (bool end = false; !end;) {
                primary_queue_.Dequeue([&](const StreamBuffer& buf) {
                    sink.AddBytes(buf.Read());
                    progress(buf.Read().size());
                    end = buf.End();
                });
            }
        };

        // We run the source on a worker thread and the sink on this thread.
        // That way, we can simply return without additional synchronization
        // once the sink work is done.
        worker_[0].Do(source_work);
        sink_work();
    }

    void ForkedStream(ForkedStreamArgs args) override {
        // Clear queues in case an earlier operation was interrupted.
        primary_queue_.Clear();
        secondary_queue_.Clear();

        // We call the caller's callbacks from different threads. To guarantee
        // that calls are sequential, we protect the calls with a mutex.
        absl::Mutex callback_mutex;

        // If and when we discover that we don't care about finishing the
        // secondary stream, we set this flag to true.
        std::atomic<bool> cancel_secondary_sink = false;

        // Latches that get flipped when the sinks have eaten all their bytes.
        Latch primary_sink_finished;
        Latch secondary_sink_finished;

        auto source_work = [&] {
            std::optional<std::int64_t> secondary_restart_position;

            // Stream data from `source` to `primary_sink`, and also to
            // `secondary_sink` as long as it can keep up.
            for (bool end = false; !end;) {
                const std::int64_t pos = args.source.GetPosition();
                primary_queue_.Enqueue([&](StreamBuffer& buf1) {
                    auto result =
                        FillBufferFromStream(args.source, buf1.Write());
                    buf1.FinishWrite(
                        {.size = result.num_bytes, .end = result.end});
                    end = result.end;
                    if (!secondary_restart_position.has_value()) {
                        const bool success =
                            secondary_queue_.NonblockingEnqueue(
                                [&](StreamBuffer& buf2) {
                                    FRZ_ASSERT_LE(buf1.Read().size(),
                                                  buf2.Write().size());
                                    std::ranges::copy(buf1.Read(),
                                                      buf2.Write().data());
                                    buf2.FinishWrite({.size = result.num_bytes,
                                                      .end = result.end});
                                    FRZ_ASSERT_EQ(buf1.Read().size(),
                                                  buf2.Read().size());
                                });
                        if (!success) {
                            // We couldn't stream the data to `secondary_sink`
                            // without blocking. Remember the position we need
                            // to restart from in case we want to finish it
                            // later.
                            secondary_restart_position = pos;
                        }
                    }
                });
            }

            // Wait for the primary sink to eat all the bytes we sent it, then
            // tell the caller that the primary stream is done.
            primary_sink_finished.Wait();
            const SecondaryStreamDecision ssd = [&] {
                absl::MutexLock ml(&callback_mutex);
                return args.primary_done();
            }();

            if (ssd == SecondaryStreamDecision::kAbandon) {
                // The caller doesn't want the secondary stream. Ask it to stop
                // ASAP and not finish flushing its buffer.
                cancel_secondary_sink.store(true, std::memory_order_relaxed);
                if (secondary_restart_position.has_value()) {
                    // The secondary stream may be blocked reading its buffer.
                    // Feed it an artificial end marker.
                    secondary_queue_.Enqueue([&](StreamBuffer& buf) {
                        buf.FinishWrite({.size = 0, .end = true});
                    });
                }
            } else if (ssd == SecondaryStreamDecision::kFinish &&
                       secondary_restart_position.has_value()) {
                // The callers wants the secondary stream, but we haven't
                // finished feeding its buffer. Do that.
                args.source.SetPosition(*secondary_restart_position);
                for (bool end = false; !end;) {
                    secondary_queue_.Enqueue([&](StreamBuffer& buf) {
                        auto result =
                            FillBufferFromStream(args.source, buf.Write());
                        buf.FinishWrite(
                            {.size = result.num_bytes, .end = result.end});
                        end = result.end;
                    });
                }
            }

            // Wait for the secondary sink to finish.
            secondary_sink_finished.Wait();
        };

        auto primary_sink_work = [&] {
            for (bool end = false; !end;) {
                primary_queue_.Dequeue([&](const StreamBuffer& buf) {
                    args.primary_sink.AddBytes(buf.Read());
                    end = buf.End();
                    absl::MutexLock ml(&callback_mutex);
                    args.primary_progress(buf.Read().size());
                });
            }
            primary_sink_finished.CountDown();
        };

        auto secondary_sink_work = [&] {
            for (bool end = false; !end && !cancel_secondary_sink.load(
                                               std::memory_order_relaxed);) {
                secondary_queue_.Dequeue([&](const StreamBuffer& buf) {
                    args.secondary_sink.AddBytes(buf.Read());
                    end = buf.End();
                    absl::MutexLock ml(&callback_mutex);
                    args.secondary_progress(buf.Read().size());
                });
            }
            secondary_sink_finished.CountDown();
        };

        // `source_work` waits for both sinks to finish, so running the sinks
        // on worker threads and the source on the current thread ensures that
        // all work is finished before we return.
        worker_[0].Do(primary_sink_work);
        worker_[1].Do(secondary_sink_work);
        source_work();
    }

  private:
    StreamBufferQueue primary_queue_;
    StreamBufferQueue secondary_queue_;
    Worker worker_[2];
};

}  // namespace

FillBufferFromStreamResult FillBufferFromStream(StreamSource& source,
                                                std::span<std::byte> buffer) {
    int num_bytes = 0;
    while (num_bytes < std::ssize(buffer)) {
        const auto result = source.GetBytes(buffer.subspan(num_bytes));
        if (auto* bw = std::get_if<StreamSource::BytesCopied>(&result)) {
            num_bytes += bw->num_bytes;
        } else if (std::get_if<StreamSource::End>(&result)) {
            return {.num_bytes = num_bytes, .end = true};
        } else {
            FRZ_CHECK(false);
        }
    }
    FRZ_ASSERT_EQ(num_bytes, buffer.size());
    return {.num_bytes = num_bytes, .end = false};
}

std::unique_ptr<Streamer> CreateSingleThreadedStreamer(
    CreateSingleThreadedStreamerArgs args) {
    return std::make_unique<SingleThreadedStreamer>(args);
}

std::unique_ptr<Streamer> CreateMultiThreadedStreamer(
    CreateMultiThreadedStreamerArgs args) {
    return std::make_unique<MultiThreadedStreamer>(args);
}

}  // namespace frz
