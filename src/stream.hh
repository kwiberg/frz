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

#include <absl/base/thread_annotations.h>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <variant>

namespace frz {

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
};

// Read from bytes from `source` and write them to `buffer`. In the return
// value, at least one of .num_bytes == buffer.size() and .end will be true.
struct FillBufferFromStreamResult {
    int num_bytes;  // Number of bytes written to `buffer`.
    bool end;       // Did we exhaust `source`?
};
FillBufferFromStreamResult FillBufferFromStream(StreamSource& source,
                                                std::span<std::byte> buffer);

class StreamSink {
  public:
    virtual ~StreamSink() = default;

    // Copy bytes from the buffer to the sink.
    virtual void AddBytes(std::span<const std::byte> buffer) = 0;
};

class Streamer {
  public:
    virtual ~Streamer() = default;

    // Stream bytes from source to sink until the former is exhausted.
    void Stream(StreamSource& source, StreamSink& sink) {
        Stream(source, sink, [](int /*num_bytes*/) {});
    }

    // Stream bytes from source to sink until the former is exhausted. Call the
    // progress callback each time a chunk is passed from source to sink.
    virtual void Stream(StreamSource& source, StreamSink& sink,
                        std::function<void(int num_bytes)> progress) = 0;
};

// Create a streamer that will alternate calls to the given source and sink,
// with a buffer of the specified size.
struct CreateSingleThreadedStreamerArgs {
    int buffer_size;
};
std::unique_ptr<Streamer> CreateSingleThreadedStreamer(
    CreateSingleThreadedStreamerArgs args);

// Create a streamer that will run the source and the sink in parallel, one on
// the current thread and the other on a worker thread.
struct CreateMultiThreadedStreamerArgs {
    int num_buffers;
    int bytes_per_buffer;
};
std::unique_ptr<Streamer> CreateMultiThreadedStreamer(
    CreateMultiThreadedStreamerArgs args);

}  // namespace frz

#endif  // FRZ_STREAM_HH_
