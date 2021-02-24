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

#include "content_source.hh"

#include <absl/container/flat_hash_map.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "content_store.hh"
#include "file_source.hh"
#include "hash.hh"
#include "hasher.hh"
#include "log.hh"
#include "stream.hh"

namespace frz {
namespace {

// A content source based on a directory tree of files. Starts out knowing only
// the set of files and their file sizes (which can be obtained by a relatively
// quick directory traversal), and lazily computes content hashes as necessary.
// In particular, since callers ask for content by hash *and size*, this
// content source is able to avoid computing hashes for any files that don't
// have the requested file size.
template <int HashBits>
class DirectoryContentSource final : public ContentSource<HashBits> {
  public:
    DirectoryContentSource(
        const std::filesystem::path& dir, bool read_only, Streamer& streamer,
        std::function<std::unique_ptr<Hasher<256>>()> create_hasher)
        : dir_(dir),
          read_only_(read_only),
          streamer_(streamer),
          create_hasher_(std::move(create_hasher)) {}

    std::optional<std::filesystem::path> Fetch(
        Log& log, const HashAndSize<HashBits>& hs,
        ContentStore& content_store) override {
        ListFiles(log);
        std::filesystem::path* const p = FindFile(log, hs);
        return p == nullptr
                   ? std::nullopt
                   : std::optional(read_only_ ? content_store.CopyInsert(*p)
                                              : content_store.MoveInsert(*p));
    }

  private:
    // Traverse the directory tree and populate files_by_size_.
    void ListFiles(Log& log) {
        if (files_listed_) {
            return;
        }
        auto progress = log.Progress("Listing files in %s", dir_);
        auto file_counter = progress.AddCounter("files");
        for (const std::filesystem::directory_entry& dent :
             std::filesystem::recursive_directory_iterator(dir_)) {
            if (dent.is_regular_file()) {
                // A regular file, or a symlink to one.
                files_by_size_[dent.file_size()].push_back(dent.path());
                file_counter.Increment(1);
            }
        }
        files_listed_ = true;
    }

    // Locate a file with the given hash+size, and return its path---or
    // nullptr, if it cannot be found. In the process, move files from
    // files_by_size_ to files_by_hash_ as their hashes become known.
    std::filesystem::path* FindFile(Log& log, const HashAndSize<HashBits>& hs) {
        auto hash_it = files_by_hash_.find(hs);
        if (hash_it != files_by_hash_.end()) {
            return &hash_it->second;
        }
        auto size_it = files_by_size_.find(hs.GetSize());
        if (size_it == files_by_size_.end()) {
            return nullptr;
        }
        FRZ_ASSERT(!size_it->second.empty());
        auto progress = log.Progress("Hashing files");
        auto file_counter = progress.AddCounter("files");
        auto byte_counter =
            progress.AddCounter("bytes", hs.GetSize() * size_it->second.size());
        while (!size_it->second.empty()) {
            std::filesystem::path p = std::move(size_it->second.back());
            size_it->second.pop_back();
            auto source = CreateFileSource(p);
            SizeHasher hasher(create_hasher_());
            streamer_.Stream(*source, hasher, [&](int num_bytes) {
                byte_counter.Increment(num_bytes);
            });
            HashAndSize<256> p_hs = hasher.Finish();
            auto [it, inserted] = files_by_hash_.insert({p_hs, std::move(p)});
            if (p_hs == hs) {
                if (size_it->second.empty()) {
                    files_by_size_.erase(size_it);
                }
                return &it->second;
            }
            file_counter.Increment(1);
        }
        FRZ_ASSERT(size_it->second.empty());
        files_by_size_.erase(size_it);
        return nullptr;
    }

    // Map from content hash+size to the path of a file with that hash+size.
    absl::flat_hash_map<HashAndSize<HashBits>, std::filesystem::path>
        files_by_hash_;

    // Map from file size to vector of paths of files of that size. Only files
    // not listed in `files_by_hash_` are listed here. Vectors are never empty.
    absl::flat_hash_map<std::uintmax_t, std::vector<std::filesystem::path>>
        files_by_size_;

    // Have we traversed the directory tree and populated files_by_size_? (We
    // do this the first time we need it rather than in the constructor, in
    // order to save time if no one ever calls us asking for any content.)
    bool files_listed_ = false;

    const std::filesystem::path dir_;
    const bool read_only_;
    Streamer& streamer_;
    const std::function<std::unique_ptr<Hasher<256>>()> create_hasher_;
};

}  // namespace

std::unique_ptr<ContentSource<256>> CreateDirectoryContentSource(
    const std::filesystem::path& dir, bool read_only, Streamer& streamer,
    std::function<std::unique_ptr<Hasher<256>>()> create_hasher) {
    return std::make_unique<DirectoryContentSource<256>>(
        dir, read_only, streamer, std::move(create_hasher));
}

}  // namespace frz
