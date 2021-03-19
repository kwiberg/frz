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

#ifndef FRZ_STREAM_HH_
#define FRZ_STREAM_HH_

/*

  This file defines interfaces and files for dealing with streams: sequences of
  bytes that go from one place (the "source") to another (the "sink").

  The general design is that both sources and sinks are passive (i.e., you make
  synchronous calls to them in order to get bytes in or out), and that an
  active actor in the middle drives them both.

*/

#include <absl/base/thread_annotations.h>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <variant>

namespace frz {

// Interface for stream sources, i.e. objects that produce a stream of bytes. A
// source will produce a finite number of bytes, and then end.
class StreamSource {
  public:
    struct BytesCopied {
        int num_bytes;
    };
    struct End {};

    virtual ~StreamSource() = default;

    // Copy bytes from the source to the buffer. Return the number of bytes
    // copied, or an end marker. May copy any number of bytes in the range [0,
    // buffer.size()].
    virtual std::variant<BytesCopied, End> GetBytes(
        std::span<std::byte> buffer) = 0;

    // Get and set the current stream position.
    virtual std::int64_t GetPosition() const = 0;
    virtual void SetPosition(std::int64_t pos) = 0;
};

// Read from bytes from `source` and write them to `buffer`. In the return
// value, at least one of .num_bytes == buffer.size() and .end will be true.
struct FillBufferFromStreamResult {
    int num_bytes;  // Number of bytes written to `buffer`.
    bool end;       // Did we exhaust `source`?
};
FillBufferFromStreamResult FillBufferFromStream(StreamSource& source,
                                                std::span<std::byte> buffer);

// Interface for stream sinks, i.e. objects that consume a stream of bytes. A
// sink must accept any number of bytes.
class StreamSink {
  public:
    virtual ~StreamSink() = default;

    // Copy bytes from the buffer to the sink.
    virtual void AddBytes(std::span<const std::byte> buffer) = 0;
};

// Interface for an object that can read bytes from a source and feed them to a
// sink. The Streamer can be reused for several source+sink pairs.
class Streamer {
  public:
    virtual ~Streamer() = default;

    // Stream bytes from source to sink until the former is exhausted.
    void Stream(StreamSource& source, StreamSink& sink) {
        Stream(source, sink, [](int /*num_bytes*/) {});
    }

    // Stream bytes from `source` to `sink` until the former is exhausted. Call
    // the progress callback each time a chunk is passed from source to sink.
    virtual void Stream(StreamSource& source, StreamSink& sink,
                        std::function<void(int num_bytes)> progress) = 0;

    // Stream bytes from `source` to `primary_sink` until the former is
    // exhausted. Also stream the bytes to `secondary_sink`, but if it can't
    // keep up with the stream from `source` to `primary_sink`, stop feeding
    // it. When `primary_sink` has received all the bytes, call `primary_done`;
    // its return value decides whether we should stop there, or rewind
    // `source` and feed `secondary_sink` any bytes it's still missing. Call
    // the respective progress callback each time a chunk is passed to one of
    // the sinks.
    enum class SecondaryStreamDecision { kAbandon, kFinish };
    struct ForkedStreamArgs {
        StreamSource& source;
        StreamSink& primary_sink;
        StreamSink& secondary_sink;
        std::function<SecondaryStreamDecision()> primary_done;
        std::function<void(int num_bytes)> primary_progress;
        std::function<void(int num_bytes)> secondary_progress;
    };
    virtual void ForkedStream(ForkedStreamArgs args) = 0;
};

// Create a streamer that will alternate calls to the given sources and sinks.
struct CreateSingleThreadedStreamerArgs {
    int buffer_size;
};
std::unique_ptr<Streamer> CreateSingleThreadedStreamer(
    CreateSingleThreadedStreamerArgs args);

// Create a streamer that will run sources and sinks in parallel. The
// parallelism is hidden, so that the caller doesn't need to worry about it
// (except if the sources and sinks share unsynchronized state---don't do
// that!).
struct CreateMultiThreadedStreamerArgs {
    int bytes_per_buffer;

    // Number of buffer chunks of size `bytes_per_buffer` for straight streams,
    // and for the primary sink in a forked stream.
    int num_buffers;

    // Number of buffer chunks of size `bytes_per_buffer` for the secondary
    // sink in a forked stream.
    int num_buffers_secondary;
};
std::unique_ptr<Streamer> CreateMultiThreadedStreamer(
    CreateMultiThreadedStreamerArgs args);

}  // namespace frz

#endif  // FRZ_STREAM_HH_
