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

#ifndef FRZ_CONTENT_STORE_HH_
#define FRZ_CONTENT_STORE_HH_

#include <filesystem>
#include <functional>
#include <memory>

#include "stream.hh"

namespace frz {

class ContentStore {
  public:
    // Use the given directory as a content store. The directory need not
    // exist; it will be created if necessary.
    static std::unique_ptr<ContentStore> Create(
        const std::filesystem::path& content_dir);

    virtual ~ContentStore() = default;

    // Stream a file into the content store. The entire streaming process must
    // take place inside the `stream_fun` callback; if the callback returns
    // true, the new file is kept, but if it returns false, it is discarded.
    // Return the path of the new file, or nullopt if `stream_fun` returned
    // false.
    virtual std::optional<std::filesystem::path> StreamInsert(
        std::function<bool(StreamSink& sink)> stream_fun) = 0;

    // Copy the given file into the content store. Return the new path.
    std::filesystem::path CopyInsert(const std::filesystem::path& source,
                                     Streamer& streamer);

    // Move the given file into the content store, falling back to copying if
    // source and destination are on different filesystems or if the source is
    // not a regular file. Return the new path.
    virtual std::filesystem::path MoveInsert(
        const std::filesystem::path& source, Streamer& streamer) = 0;

    // Iterate over all regular files in the content store. The callback is
    // given two handles to each content file: `dent`, a directory entry whose
    // path is either absolute or relative to the current working directory,
    // and `canonical_path`, which is relative to the root directory of the
    // content store (the same as the return value of `CanonicalPath`).
    virtual void ForEach(
        std::function<void(const std::filesystem::directory_entry& dent,
                           const std::filesystem::path& canonical_path)>
            callback) const = 0;

    // Given a path `file`: if it belongs to the content store, return it in
    // canonical form relative to the root directory of the content store; if
    // it doesn't belong to the content store, return nullopt.
    virtual std::optional<std::filesystem::path> CanonicalPath(
        const std::filesystem::path& file) const = 0;
};

}  // namespace frz

#endif  // FRZ_CONTENT_STORE_HH_
