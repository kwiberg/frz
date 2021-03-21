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
#include "exceptions.hh"
#include "file_stream.hh"
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
        ContentStore& content_store) override try {
        ListFiles(log);
        std::optional<FindFileResult> r =
            FindFile(log, hs, read_only_ ? &content_store : nullptr);
        if (!r.has_value()) {
            // Couldn't find the requested content.
            return std::nullopt;
        } else if (r->already_inserted) {
            // FindFile() inserted the content for us.
            return r->path;
        } else {
            // FindFile() found the content, and we need to insert it.
            return read_only_ ? content_store.CopyInsert(r->path, streamer_)
                              : content_store.MoveInsert(r->path, streamer_);
        }
    } catch (const Error& e) {
        log.Important("When fetching %s: %s", hs.ToBase32(), e.what());
        return std::nullopt;
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
            if (std::filesystem::is_regular_file(dent.symlink_status())) {
                // A regular file (not a symlink to one).
                files_by_size_[dent.file_size()].push_back(dent.path());
                file_counter.Increment(1);
            }
        }
        files_listed_ = true;
    }

    // Locate a file with the given hash+size, and return its path---or
    // nullopt, if it cannot be found. In the process, move file paths from
    // `files_by_size_` to `files_by_hash_` as their hashes become known. In
    // case it's efficient to do so, stream-insert the file to `content_store`
    // as part of the search.
    struct FindFileResult {
        // The path where the requested file can be found.
        std::filesystem::path path;

        // Did we insert the file into the content store?
        bool already_inserted;
    };
    std::optional<FindFileResult> FindFile(Log& log,
                                           const HashAndSize<HashBits>& hs,
                                           ContentStore* const content_store) {
        auto hash_it = files_by_hash_.find(hs);
        if (hash_it != files_by_hash_.end()) {
            return FindFileResult{.path = hash_it->second,
                                  .already_inserted = false};
        }
        auto size_it = files_by_size_.find(hs.GetSize());
        if (size_it == files_by_size_.end()) {
            return std::nullopt;
        }
        FRZ_ASSERT(!size_it->second.empty());
        auto progress = log.Progress("Hashing files");
        auto file_counter = progress.AddCounter("files");
        auto byte_counter =
            progress.AddCounter("bytes", hs.GetSize() * size_it->second.size());
        while (!size_it->second.empty()) {
            std::filesystem::path p = std::move(size_it->second.back());
            size_it->second.pop_back();
            try {
                auto source = CreateFileSource(p);
                SizeHasher hasher(create_hasher_());
                std::optional<HashAndSize<256>> p_hs;
                std::optional<std::filesystem::path> inserted_path;
                if (content_store == nullptr) {
                    streamer_.Stream(*source, hasher, [&](int num_bytes) {
                        byte_counter.Increment(num_bytes);
                    });
                    p_hs = hasher.Finish();
                } else {
                    inserted_path = content_store->StreamInsert(
                        [&](StreamSink& content_sink) {
                            // Stream the file contents to both the hasher and
                            // the content store. We wait for the secondary
                            // transfer to finish iff the hash was the one we
                            // were looking for.
                            auto kFinish =
                                Streamer::SecondaryStreamDecision::kFinish;
                            auto kAbandon =
                                Streamer::SecondaryStreamDecision::kAbandon;
                            streamer_.ForkedStream(
                                {.source = *source,
                                 .primary_sink = hasher,
                                 .secondary_sink = content_sink,
                                 .primary_done =
                                     [&] {
                                         p_hs = hasher.Finish();
                                         return p_hs == hs ? kFinish : kAbandon;
                                     },
                                 .primary_progress =
                                     [&](int num_bytes) {
                                         byte_counter.Increment(num_bytes);
                                     },
                                 .secondary_progress =
                                     [](int /*num_bytes*/) {}});
                            return p_hs == hs;  // keep the inserted content iff
                                                // the hash matched
                        });
                }
                FRZ_ASSERT(p_hs.has_value());
                auto [it, inserted] =
                    files_by_hash_.insert({*p_hs, std::move(p)});
                if (p_hs == hs) {
                    if (size_it->second.empty()) {
                        files_by_size_.erase(size_it);
                    }
                    return FindFileResult{
                        .path = inserted_path.value_or(it->second),
                        .already_inserted = inserted_path.has_value()};
                }
            } catch (const Error& e) {
                log.Important("When reading %s: %s", p, e.what());
            }
            file_counter.Increment(1);
        }
        FRZ_ASSERT(size_it->second.empty());
        files_by_size_.erase(size_it);
        return std::nullopt;
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

template <int HashBits>
std::unique_ptr<ContentSource<HashBits>> ContentSource<HashBits>::Create(
    const std::filesystem::path& dir, bool read_only, Streamer& streamer,
    std::function<std::unique_ptr<Hasher<HashBits>>()> create_hasher) {
    return std::make_unique<DirectoryContentSource<HashBits>>(
        dir, read_only, streamer, std::move(create_hasher));
}

template class ContentSource<256>;

}  // namespace frz
