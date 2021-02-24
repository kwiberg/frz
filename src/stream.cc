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

#include "assert.hh"
#include "worker.hh"

namespace frz {
namespace {

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

  private:
    const std::unique_ptr<std::byte[]> buffer_;
    const std::size_t buffer_size_;
};

class StreamBuffer {
  public:
    static std::vector<StreamBuffer> CreateVector(int num_buffers,
                                                  int bytes_per_buffer) {
        std::vector<StreamBuffer> v;
        v.reserve(num_buffers);
        for (int i = 0; i < num_buffers; ++i) {
            v.emplace_back(bytes_per_buffer);
        }
        return v;
    }

    StreamBuffer(int size)
        : data_(std::make_unique<std::byte[]>(size)), capacity_(size) {}

    StreamBuffer(StreamBuffer&&) = default;
    StreamBuffer& operator=(StreamBuffer&&) = default;
    StreamBuffer(const StreamBuffer&) = delete;
    StreamBuffer& operator=(const StreamBuffer&) = delete;

    std::span<const std::byte> Read() const {
        return std::span(data_.get(), status_.size);
    }

    bool End() const { return status_.end; }

    std::span<std::byte> Write() { return std::span(data_.get(), capacity_); }

    struct WriteStatus {
        int size;
        bool end;
    };
    void FinishWrite(WriteStatus status) {
        FRZ_ASSERT_LE(status.size, capacity_);
        status_ = status;
    }

  private:
    std::unique_ptr<std::byte[]> data_;
    int capacity_;
    WriteStatus status_ = {.size = 0, .end = false};
};

// TODO: Replace with `using Semaphore = std::counting_semaphore<100>` once we
// have standard library support for it.
class Semaphore final {
  public:
    static constexpr std::ptrdiff_t max() noexcept { return 100; }

    explicit Semaphore(int initial_count) : count_(initial_count) {}

    void acquire() {
        auto not_blocked = [&] { return count_ >= 1; };
        absl::MutexLock ml(&mutex_, absl::Condition(&not_blocked));
        --count_;
    }

    void release() {
        absl::MutexLock ml(&mutex_);
        ++count_;
    }

  private:
    absl::Mutex mutex_;
    int count_ ABSL_GUARDED_BY(mutex_);
};

class MultiThreadedStreamer final : public Streamer {
  public:
    MultiThreadedStreamer(CreateMultiThreadedStreamerArgs args)
        : buffers_(StreamBuffer::CreateVector(args.num_buffers,
                                              args.bytes_per_buffer)) {}

    void Stream(StreamSource& source, StreamSink& sink,
                std::function<void(int num_bytes)> progress) override {
        FRZ_ASSERT_LE(buffers_.size(), Semaphore::max());
        Semaphore free_buffers(FRZ_ASSERT_CAST(int, buffers_.size()));
        Semaphore full_buffers(0);

        auto source_work = [&] {
            // Loop over the buffers, starting over from the beginning when we
            // reach the end.
            for (int i = 0; true; ++i, i = i < std::ssize(buffers_) ? i : 0) {
                // Acquire buffers_[i] from the "free" queue, blocking until it
                // becomes available.
                free_buffers.acquire();

                // Fill buffers_[i] with bytes and release it to the "full"
                // queue.
                auto result = FillBufferFromStream(source, buffers_[i].Write());
                buffers_[i].FinishWrite(
                    {.size = result.num_bytes, .end = result.end});
                full_buffers.release();

                if (result.end) {
                    return;  // The stream ended.
                }
            }
        };

        auto sink_work = [&] {
            // Loop over the buffers, starting over from the beginning when we
            // reach the end.
            for (int i = 0; true; ++i, i = i < std::ssize(buffers_) ? i : 0) {
                // Acquire buffers_[i] from the "full" queue, blocking until it
                // becomes available.
                full_buffers.acquire();

                // Feed the contents of buffers_[i] to the sink.
                sink.AddBytes(buffers_[i].Read());
                progress(buffers_[i].Read().size());
                const bool end = buffers_[i].End();

                // Release buffers_[i] to the "free" queue.
                free_buffers.release();

                if (end) {
                    return;  // The stream ended.
                }
            }
        };

        worker_.Do(source_work);
        sink_work();
    }

  private:
    std::vector<StreamBuffer> buffers_;
    Worker worker_;
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
