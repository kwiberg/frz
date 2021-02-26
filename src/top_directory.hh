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

#ifndef FRZ_TOP_DIRECTORY_HH_
#define FRZ_TOP_DIRECTORY_HH_

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include "hasher.hh"
#include "log.hh"
#include "stream.hh"

namespace frz {

// Instances of this class represent zero or more Frz repositories, which are
// automatically discovered by looking in the argument directory, the parent
// directory, the grandparent directory, etc.
class Top {
  public:
    struct ContentSource {
        std::filesystem::path path;
        bool read_only;
    };

    static std::unique_ptr<Top> Create(
        Streamer& streamer,
        std::function<std::unique_ptr<Hasher<256>>()> create_hasher,
        std::string hash_name);

    virtual ~Top() = default;

    // Add the given file (move it to the content directory, and replace it
    // with a symlink). Return true if the file contents were used, false if
    // they were ignored because the content directory already had an identical
    // file.
    enum class AddResult {
        kNewFile,
        kDuplicateFile,
        kSymlink,
    };
    virtual AddResult AddFile(const std::filesystem::path& file) = 0;

    // Identify and attempt to fill missing content in the frz repository that
    // owns `path`. `content_sources` lists directories that we may copy or
    // move files from.
    struct FillResult {
        // Number of missing content files that we were able to fetch.
        std::int64_t num_fetched = 0;

        // Number of content files that are still missing.
        std::int64_t num_still_missing = 0;
    };
    virtual FillResult Fill(Log& log, const std::filesystem::path& path,
                            std::vector<ContentSource> content_sources) = 0;

    // Fix problems with the frz repository that owns `path`. In case content
    // is missing, `content_sources` lists directories that we may copy or move
    // files from.
    struct RepairResult {
        // The number of index symlinks that point to good content. (We kept
        // these.)
        std::int64_t num_good_index_symlinks = 0;

        // The number of index symlinks that didn't point to the content they
        // were supposed to. (We removed these.)
        std::int64_t num_bad_index_symlinks = 0;

        // The number of content files that didn't have index symlinks. (Now
        // they do.)
        std::int64_t num_missing_index_symlinks = 0;

        // The number of content files that couldn't be given index symlinks,
        // because they have the same hash as other content files. (We moved
        // these to unused-content/.)
        std::int64_t num_duplicate_content_files = 0;

        // Number of missing content files that we were able to fetch.
        std::int64_t num_fetched = 0;

        // Number of content files that are still missing.
        std::int64_t num_still_missing = 0;
    };
    virtual RepairResult Repair(Log& log, const std::filesystem::path& path,
                                bool verify_all_hashes,
                                std::vector<ContentSource> content_sources) = 0;
};

}  // namespace frz

#endif  // FRZ_TOP_DIRECTORY_HH_
