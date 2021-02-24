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

namespace frz {

class ContentStore {
  public:
    virtual ~ContentStore() = default;

    // Copy the given file into the content store. Return the new path.
    virtual std::filesystem::path CopyInsert(
        const std::filesystem::path& source) = 0;

    // Move the given file into the content store, falling back to copying if
    // source and destination are on different filesystems or if the source is
    // not a regular file. Return the new path.
    virtual std::filesystem::path MoveInsert(
        const std::filesystem::path& source) = 0;

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

std::unique_ptr<ContentStore> CreateDiskContentStore(
    const std::filesystem::path& content_dir);

}  // namespace frz

#endif  // FRZ_CONTENT_STORE_HH_
