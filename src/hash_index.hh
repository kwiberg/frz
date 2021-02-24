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

#ifndef FRZ_HASH_INDEX_HH_
#define FRZ_HASH_INDEX_HH_

#include <filesystem>
#include <functional>
#include <memory>

#include "hash.hh"
#include "log.hh"

namespace frz {

template <int HashBits>
class HashIndex {
  public:
    virtual ~HashIndex() = default;

    // Insert a new path. Return true if the insertion succeeded, false if the
    // hash was already present.
    virtual bool Insert(const HashAndSize<HashBits>& hs,
                        const std::filesystem::path& path) = 0;

    // Does the index have an entry for the given hash?
    virtual bool Contains(const HashAndSize<HashBits>& hs) const = 0;

    // Remove junk from the index. Any entries that aren't syntactically valid
    // are removed; for the entries that are syntactically valid, the supplied
    // callback decides whether to keep them or not.
    virtual void Scrub(Log& log,
                       std::function<bool(const HashAndSize<HashBits>& hs,
                                          const std::filesystem::path& path)>
                           is_good) = 0;
};

std::unique_ptr<HashIndex<256>> CreateRamHashIndex();

std::unique_ptr<HashIndex<256>> CreateDiskHashIndex(
    const std::filesystem::path& index_dir);

}  // namespace frz

#endif  // FRZ_HASH_INDEX_HH_
