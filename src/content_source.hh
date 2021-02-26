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

#ifndef FRZ_CONTENT_SOURCE_HH_
#define FRZ_CONTENT_SOURCE_HH_

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "content_store.hh"
#include "hash.hh"
#include "hasher.hh"
#include "log.hh"
#include "stream.hh"

namespace frz {

template <int HashBits>
class ContentSource {
  public:
    // Use the given directory as a content source.
    static std::unique_ptr<ContentSource<HashBits>> Create(
        const std::filesystem::path& dir, bool read_only, Streamer& streamer,
        std::function<std::unique_ptr<Hasher<HashBits>>()> create_hasher);

    virtual ~ContentSource() = default;

    // Fetch a file with the given hash from the content source, and put in in
    // the given content store. Return the path of the inserted file, or
    // nullopt if such a file was not available.
    virtual std::optional<std::filesystem::path> Fetch(
        Log& log, const HashAndSize<HashBits>& hs,
        ContentStore& content_store) = 0;
};

// Instantiated for `HashBits` == 256. Add more instantiations here if they are
// needed.
extern template class ContentSource<256>;

}  // namespace frz

#endif  // FRZ_CONTENT_SOURCE_HH_
