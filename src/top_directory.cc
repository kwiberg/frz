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

#include "top_directory.hh"

#include <absl/container/flat_hash_set.h>
#include <absl/container/node_hash_map.h>
#include <filesystem>
#include <memory>
#include <utility>

#include "assert.hh"
#include "content_source.hh"
#include "content_store.hh"
#include "exceptions.hh"
#include "file_source.hh"
#include "filesystem_util.hh"
#include "hash_index.hh"
#include "hasher.hh"
#include "log.hh"
#include "stream.hh"

namespace frz {
namespace {

bool IsTopDir(const std::filesystem::directory_entry& dent) {
    return std::filesystem::is_directory(dent.symlink_status()) &&
           std::filesystem::is_directory(
               std::filesystem::symlink_status(dent.path() / ".frz"));
}

bool IsTopDir(const std::filesystem::path& dir) {
    return IsTopDir(std::filesystem::directory_entry(dir));
}

class TopDirectory final {
  public:
    TopDirectory(const std::filesystem::path& path, Streamer& streamer,
                 std::function<std::unique_ptr<Hasher<256>>()> create_hasher,
                 std::string hash_name)
        : path_(path),
          hash_index_(CreateDiskHashIndex(path / ".frz" / hash_name)),
          content_store_(ContentStore::Create(path / ".frz" / "content")),
          unused_content_store_(
              ContentStore::Create(path / ".frz" / "unused-content")),
          streamer_(streamer),
          create_hasher_(std::move(create_hasher)),
          hash_name_(std::move(hash_name)) {}

    Top::AddResult AddFile(const std::filesystem::path& file,
                           int subdir_levels) {
        CreateHashdirSymlink(file.parent_path(), subdir_levels);
        if (std::filesystem::is_symlink(file)) {
            return Top::AddResult::kSymlink;
        }
        FRZ_ASSERT(std::filesystem::is_regular_file(
            std::filesystem::symlink_status(file)));
        auto source = CreateFileSource(file);
        SizeHasher hasher(create_hasher_());
        streamer_.Stream(*source, hasher);
        HashAndSize<256> hs = hasher.Finish();
        const std::string base32 = hs.ToBase32();
        const std::filesystem::path file2 = TempFilename(file, base32);
        std::filesystem::rename(file, file2);
        std::filesystem::create_symlink(SymlinkTarget(base32), file);
        const std::filesystem::path content_path =
            content_store_->MoveInsert(file2);
        const bool inserted = hash_index_->Insert(hs, content_path);
        if (!inserted) {
            unused_content_store_->MoveInsert(content_path);
        }
        return inserted ? Top::AddResult::kNewFile
                        : Top::AddResult::kDuplicateFile;
    }

    Top::FillResult Fill(Log& log,
                         std::vector<Top::ContentSource> content_sources) {
        auto r = FetchMissingContent(log, std::move(content_sources));
        return {.num_fetched = r.num_fetched,
                .num_still_missing = r.num_still_missing};
    }

    Top::RepairResult Repair(Log& log, bool verify_all_hashes,
                             std::vector<Top::ContentSource> content_sources) {
        auto r1 = CheckIndexSymlinks(log, verify_all_hashes);
        auto r2 = CheckContentFiles(log, r1.indexed_content_files);
        auto r3 = FetchMissingContent(log, std::move(content_sources));
        return {.num_good_index_symlinks = r1.num_good_index_symlinks,
                .num_bad_index_symlinks = r1.num_bad_index_symlinks,
                .num_missing_index_symlinks = r2.num_missing_index_symlinks,
                .num_duplicate_content_files = r2.num_duplicate_content_files,
                .num_fetched = r3.num_fetched,
                .num_still_missing = r3.num_still_missing};
    }

  private:
    void CreateHashdirSymlink(const std::filesystem::path& dir,
                              int subdir_levels) {
        FRZ_ASSERT(std::filesystem::is_directory(
            std::filesystem::symlink_status(dir)));
        std::filesystem::path link = dir / ".frz";
        if (subdir_levels == 0) {
            // At the top level. We need no symlink here.
            FRZ_ASSERT(std::filesystem::is_directory(
                std::filesystem::symlink_status(link)));
            return;
        }
        auto link_target = std::filesystem::path(".frz");
        for (int i = 0; i < subdir_levels; ++i) {
            link_target = ".." / link_target;
        }
        std::filesystem::file_status link_status = symlink_status(link);
        if (std::filesystem::is_symlink(link_status)) {
            if (std::filesystem::read_symlink(link) == link_target) {
                return;  // the desired symlink already exists
            } else {
                std::filesystem::remove(link);
            }
        } else if (std::filesystem::exists(link_status)) {
            throw Error(
                "Could not create symlink %s, because something with that name "
                "already exists",
                link);
        }

        std::filesystem::create_directory_symlink(link_target, link);
    }

    std::filesystem::path TempFilename(std::filesystem::path file,
                                       std::string_view base32) const {
        file += absl::StrCat(".frz-", hash_name_, "-", base32);
        return file;
    }

    std::filesystem::path SymlinkTarget(std::string_view base32) {
        return std::filesystem::path(".frz") / hash_name_ / SymlinkPath(base32);
    }

    // Check all index symlinks in the frz repository, keeping the good ones
    // and removing the bad ones. If `verify_all_hashes` is true, recompute
    // content hashes; if false, trust that content files still have the
    // correct hash.
    struct CheckIndexSymlinksResult {
        // The number of index symlinks that point to good content. (We kept
        // these.)
        std::int64_t num_good_index_symlinks = 0;

        // The number of index symlinks that didn't point to the content they
        // were supposed to. (We removed these.)
        std::int64_t num_bad_index_symlinks = 0;

        // The content files that have good index symlinks. Paths are relative
        // to the content directory.
        absl::flat_hash_set<std::string> indexed_content_files;
    };
    CheckIndexSymlinksResult CheckIndexSymlinks(Log& log,
                                                bool verify_all_hashes) {
        CheckIndexSymlinksResult result;
        auto progress = log.Progress("Checking index links and content files");
        auto symlink_counter = progress.AddCounter("links");
        auto content_file_counter = progress.AddCounter("files");
        hash_index_->Scrub(log, [&](const HashAndSize<256>& hs,
                                    const std::filesystem::path& content_path) {
            symlink_counter.Increment(1);
            std::optional<std::filesystem::path> canonical_content_path;
            try {
                canonical_content_path =
                    content_store_->CanonicalPath(content_path);
                if (!canonical_content_path.has_value()) {
                    log.Info(
                        "Removing %s from the index because it points to %s, "
                        "which is outside the content directory.",
                        hs.ToBase32(), content_path);
                    ++result.num_bad_index_symlinks;
                    return false;
                }
                std::filesystem::directory_entry dent(content_path);
                if (!dent.is_regular_file()) {
                    log.Info(
                        "Removing %s from the index because it points to %s, "
                        "which doesn't exist or isn't regular file.",
                        hs.ToBase32(), *canonical_content_path);
                    ++result.num_bad_index_symlinks;
                    return false;
                }
                if (std::cmp_not_equal(dent.file_size(), hs.GetSize())) {
                    log.Info(
                        "Removing %s from the index because it points to %s, "
                        "which has the wrong size (expected %d, actual %d).",
                        hs.ToBase32(), *canonical_content_path, hs.GetSize(),
                        dent.file_size());
                    ++result.num_bad_index_symlinks;
                    return false;
                }
                auto source = CreateFileSource(content_path);
                content_file_counter.Increment(1);
                if (verify_all_hashes) {
                    SizeHasher hasher(create_hasher_());
                    streamer_.Stream(*source, hasher);
                    HashAndSize<256> actual_hs = hasher.Finish();
                    if (actual_hs != hs) {
                        log.Info(
                            "Removing %s from the index because it points to "
                            "%s, which has the wrong hash (%s).",
                            hs.ToBase32(), *canonical_content_path,
                            actual_hs.ToBase32());
                        ++result.num_bad_index_symlinks;
                        return false;
                    }
                } else {
                    std::byte first_byte;
                    auto r = FillBufferFromStream(*source,
                                                  std::span(&first_byte, 1));
                    if (r.num_bytes == 0 && hs.GetSize() >= 1) {
                        log.Info(
                            "Removing %s from the index because it points to "
                            "%s; reading the first byte immediately hit "
                            "end-of-file.",
                            hs.ToBase32(), *canonical_content_path);
                        ++result.num_bad_index_symlinks;
                        return false;
                    }
                    if (r.num_bytes == 1 && hs.GetSize() < 1) {
                        log.Info(
                            "Removing %s from the index because it points to "
                            "%s; it's supposed to be an empty file, but "
                            "reading the first byte succeeded.",
                            hs.ToBase32(), *canonical_content_path);
                        ++result.num_bad_index_symlinks;
                        return false;
                    }
                }
            } catch (const Error& e) {
                log.Info(
                    "Removing %s from the index because it points to %s, "
                    "and we got the following error when verifying it: %s",
                    hs.ToBase32(), content_path, e.what());
                ++result.num_bad_index_symlinks;
                return false;
            }
            ++result.num_good_index_symlinks;
            result.indexed_content_files.insert(
                canonical_content_path->native());
            return true;  // Keep in index.
        });
        return result;
    }

    // Check all content files in the frz repository, adding index symlinks for
    // content files that don't have them, and moving duplicate content files
    // to unused-content/. As an optimization, the files in
    // `indexed_content_files` are trusted to have index symlinks.
    struct CheckContentFilesResult {
        // The number of content files that didn't have index symlinks. (Now
        // they do.)
        std::int64_t num_missing_index_symlinks = 0;

        // The number of content files that couldn't be given index symlinks,
        // because they have the same hash as other content files. (We moved
        // these to unused-content/.)
        std::int64_t num_duplicate_content_files = 0;
    };
    CheckContentFilesResult CheckContentFiles(
        Log& log,
        const absl::flat_hash_set<std::string>& indexed_content_files) {
        CheckContentFilesResult result;
        auto progress = log.Progress("Checking orphaned content files");
        auto file_counter = progress.AddCounter("files");
        auto byte_counter = progress.AddCounter("bytes");
        content_store_->ForEach([&](const std::filesystem::directory_entry&
                                        dent,
                                    const std::filesystem::path&
                                        canonical_path) {
            if (!IsReadonly(dent.status())) {
                log.Info("Removing write permissions from %s.", canonical_path);
                RemoveWritePermissions(dent);
            }
            if (indexed_content_files.contains(canonical_path.native())) {
                // We trust that this content file is already properly indexed.
                return;
            }
            auto source = CreateFileSource(dent);
            SizeHasher hasher(create_hasher_());
            streamer_.Stream(*source, hasher, [&](int num_bytes) {
                byte_counter.Increment(num_bytes);
            });
            const HashAndSize<256> hs = hasher.Finish();
            const bool inserted = hash_index_->Insert(hs, dent);
            if (inserted) {
                log.Info(
                    "Adding %s to the index, pointing to %s (content was "
                    "already present, but not indexed).",
                    hs.ToBase32(), canonical_path);
                ++result.num_missing_index_symlinks;
            } else {
                unused_content_store_->MoveInsert(dent);
                log.Info(
                    "Moving duplicate content file %s to unused-content/ (hash "
                    "%s).",
                    canonical_path, hs.ToBase32());
                ++result.num_duplicate_content_files;
            }
            file_counter.Increment(1);
        });
        return result;
    }

    // Fetch any missing content for the frz repository. `move_sources` lists
    // directories that we may move files from, and `copy_sources` lists
    // directories that we may only copy files from.
    struct FetchMissingContentResult {
        // Number of missing content files that we were able to fetch.
        std::int64_t num_fetched = 0;

        // Number of content files that are still missing.
        std::int64_t num_still_missing = 0;
    };
    FetchMissingContentResult FetchMissingContent(
        Log& log, std::vector<Top::ContentSource> content_sources) {
        FetchMissingContentResult result;
        auto progress =
            log.Progress("Checking that referenced content is present");
        auto symlink_counter = progress.AddCounter("links");

        // Prefer .frs/unused-content to any sources specified by the user.
        std::filesystem::path unused_content_path =
            path_ / ".frz" / "unused-content";
        if (std::filesystem::exists(unused_content_path)) {
            content_sources.insert(
                content_sources.begin(),
                {.path = unused_content_path, .read_only = false});
        }
        std::vector<std::unique_ptr<ContentSource<256>>> sources;
        for (const auto& s : content_sources) {
            sources.push_back(ContentSource<256>::Create(
                s.path, s.read_only, streamer_, create_hasher_));
        }
        FetchMissingContentDir(result, log, symlink_counter, sources,
                               std::filesystem::directory_entry(path_), 0);
        return result;
    }

    void FetchMissingContentDir(
        FetchMissingContentResult& result, Log& log,
        ProgressLogCounter& symlink_counter,
        std::span<const std::unique_ptr<ContentSource<256>>> sources,
        const std::filesystem::directory_entry& dir, const int subdir_levels) {
        if (IsTopDir(dir) && subdir_levels > 0) {
            // Ignore other repos.
            return;
        }
        bool good_hashdir_symlink = false;
        for (const std::filesystem::directory_entry& dent :
             std::filesystem::directory_iterator(dir)) {
            if (dent.path().filename() == ".frz") {
                // Ignore our own .frz directory and our .frz symlinks.
            } else if (std::filesystem::is_directory(dent.symlink_status())) {
                FetchMissingContentDir(result, log, symlink_counter, sources,
                                       dent, subdir_levels + 1);
            } else if (dent.is_symlink()) {
                // Try parsing the symlink target as a base-32 content hash; if
                // this fails, it isn't one of our symlinks, so ignore it.
                const std::optional<std::string> base32 = PathBase32(
                    hash_name_, std::filesystem::read_symlink(dent.path()));
                if (!base32.has_value()) {
                    continue;
                }
                const std::optional<HashAndSize<256>> hs =
                    HashAndSize<256>::FromBase32(*base32);
                if (!hs.has_value()) {
                    continue;
                }

                // This is one of our symlinks!
                symlink_counter.Increment(1);

                // Make sure that the .frz symlink exists in this directory...
                if (!good_hashdir_symlink) {
                    CreateHashdirSymlink(dir.path(), subdir_levels);
                    good_hashdir_symlink = true;
                }

                // ...and fetch the content if we don't already have it.
                if (!hash_index_->Contains(*hs)) {
                    bool fetched = false;
                    for (const auto& s : sources) {
                        const std::optional<std::filesystem::path>
                            content_path = s->Fetch(log, *hs, *content_store_);
                        if (content_path.has_value()) {
                            fetched = hash_index_->Insert(*hs, *content_path);
                            FRZ_ASSERT(fetched);
                            break;
                        }
                    }
                    if (fetched) {
                        ++result.num_fetched;
                    } else {
                        ++result.num_still_missing;
                    }
                }
            }
        }
    }

    const std::filesystem::path path_;
    const std::unique_ptr<HashIndex<256>> hash_index_;
    const std::unique_ptr<ContentStore> content_store_;
    const std::unique_ptr<ContentStore> unused_content_store_;
    Streamer& streamer_;
    const std::function<std::unique_ptr<Hasher<256>>()> create_hasher_;
    const std::string hash_name_;
};

class TopDirectoryCache final : public Top {
  public:
    TopDirectoryCache(
        Streamer& streamer,
        std::function<std::unique_ptr<Hasher<256>>()> create_hasher,
        std::string hash_name)
        : streamer_(streamer),
          create_hasher_(std::move(create_hasher)),
          hash_name_(std::move(hash_name)) {}

    AddResult AddFile(const std::filesystem::path& file) override {
        const TopDirRef& t = GetTopDir(file);
        return t.topdir->AddFile(file, t.level);
    }

    FillResult Fill(Log& log, const std::filesystem::path& path,
                    std::vector<ContentSource> content_sources) override {
        const TopDirRef& t = GetTopDir(path);
        return t.topdir->Fill(log, std::move(content_sources));
    }

    RepairResult Repair(Log& log, const std::filesystem::path& path,
                        bool verify_all_hashes,
                        std::vector<ContentSource> content_sources) override {
        const TopDirRef& t = GetTopDir(path);
        return t.topdir->Repair(log, verify_all_hashes,
                                std::move(content_sources));
    }

  private:
    struct TopDirRef {
        std::shared_ptr<TopDirectory> topdir;
        int level;  // how many levels down are we from the top dir?
    };

    const TopDirRef& GetTopDir(const std::filesystem::path& path) try {
        std::filesystem::path p = NonLeafCanonical(path);
        return GetTopDir(
            /*canonical_dir=*/std::filesystem::is_directory(p)
                ? p
                : p.parent_path(),
            /*original_path=*/path);
    } catch (const std::filesystem::filesystem_error& e) {
        throw Error("Found no .frz directory for %s: %s", path, e.what());
    }

    const TopDirRef& GetTopDir(const std::filesystem::path& canonical_dir,
                               const std::filesystem::path& original_path) {
        FRZ_ASSERT(std::filesystem::is_directory(
            std::filesystem::symlink_status(canonical_dir)));

        // Get a reference to the shared_ptr that holds the TopDirectory for
        // `canonical_dir`.
        TopDirRef& t = top_dirs_[canonical_dir.native()];
        if (t.topdir == nullptr) {
            // The TopDirectory pointer is null (because we just default
            // inserted it). We need to fill it in.
            if (IsTopDir(canonical_dir)) {
                t.topdir = std::make_shared<TopDirectory>(
                    canonical_dir, streamer_, create_hasher_, hash_name_);
                t.level = 0;  // we found the top dir at this level
            } else {
                auto parent_dir = canonical_dir.parent_path();
                if (std::filesystem::equivalent(parent_dir, canonical_dir)) {
                    // We've reached the root, and can't go further up.
                    throw Error("Found no .frz directory for %s",
                                original_path);
                }
                t = GetTopDir(parent_dir, original_path);
                ++t.level;  // the top dir is one level further up from here,
                            // compared to from our parent directory
            }
        }
        return t;
    }

    // Map from directory name to the TopDirectory object that owns it. This is
    // a node_hash_map rather than a flat_hash_map because we need pointer
    // stability, since we look up entries and keep references to them that
    // need to stay valid even though we insert more elements in the meantime.
    absl::node_hash_map<std::filesystem::path::string_type, TopDirRef>
        top_dirs_;

    Streamer& streamer_;
    const std::function<std::unique_ptr<Hasher<256>>()> create_hasher_;
    const std::string hash_name_;
};

}  // namespace

std::unique_ptr<Top> Top::Create(
    Streamer& streamer,
    std::function<std::unique_ptr<Hasher<256>>()> create_hasher,
    std::string hash_name) {
    return std::make_unique<TopDirectoryCache>(
        streamer, std::move(create_hasher), std::move(hash_name));
}

}  // namespace frz
