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

#include "content_store.hh"

#include <absl/random/random.h>
#include <filesystem>
#include <memory>
#include <string_view>
#include <system_error>

#include "assert.hh"
#include "base32.hh"
#include "filesystem_util.hh"

namespace frz {
namespace {

class DiskContentStore final : public ContentStore {
  public:
    DiskContentStore(const std::filesystem::path& content_dir)
        : content_dir_(content_dir) {}

    std::filesystem::path CopyInsert(
        const std::filesystem::path& source) override {
        FRZ_ASSERT(std::filesystem::is_regular_file(source));
        int depth = 0;
        while (true) {
            // Generate a destination filename, and attempt to copy `source` to
            // it.
            const std::filesystem::path destination =
                SuggestDestinationFilename(depth);
            try {
                std::filesystem::copy_file(source, destination);
            } catch (const std::filesystem::filesystem_error& e) {
                if (e.code() == std::errc::file_exists) {
                    // Collision; try another, longer, random path name.
                    continue;
                } else {
                    // Unexpected error; re-throw.
                    throw;
                }
            }
            RemoveWritePermissions(destination);
            return destination;
        }
    }

    std::filesystem::path MoveInsert(
        const std::filesystem::path& source) override {
        if (std::filesystem::is_symlink(source)) {
            // We don't want to move either the symlink or its taget, because
            // neither is likely to be what the user expects; copy instead.
            return CopyInsert(source);
        }
        FRZ_ASSERT(std::filesystem::is_regular_file(
            std::filesystem::symlink_status(source)));
        int depth = 0;
        while (true) {
            // Generate a destination filename, and attempt to move `source` to
            // it. We can't use std::filesystem::rename(), because it
            // overwrites the destination file if it already exists; instead,
            // we create a new hardlink and unlink the old one.
            const std::filesystem::path destination =
                SuggestDestinationFilename(depth);
            try {
                std::filesystem::create_hard_link(source, destination);
            } catch (const std::filesystem::filesystem_error& e) {
                if (e.code() == std::errc::file_exists) {
                    // Collision; try another, longer, random path name.
                    continue;
                } else if (e.code() == std::errc::cross_device_link) {
                    // Source and destination are on different filesystems; we
                    // need to copy instead of move.
                    return CopyInsert(source);
                } else {
                    // Unexpected error; re-throw.
                    throw;
                }
            }
            std::filesystem::remove(source);
            RemoveWritePermissions(destination);
            return destination;
        }
    }

    void ForEach(
        std::function<void(const std::filesystem::directory_entry& dent,
                           const std::filesystem::path& canonical_path)>
            callback) const override {
        if (!std::filesystem::exists(content_dir_)) {
            return;
        }
        for (const std::filesystem::directory_entry& dent :
             std::filesystem::recursive_directory_iterator(content_dir_)) {
            if (!std::filesystem::is_regular_file(dent.symlink_status())) {
                continue;
            }
            std::optional<std::filesystem::path> canonical_path =
                CanonicalPath(dent.path());
            FRZ_ASSERT(canonical_path.has_value());
            callback(dent, *canonical_path);
        }
    }

    std::optional<std::filesystem::path> CanonicalPath(
        const std::filesystem::path& file) const override {
        return RelativeSubtreePath(file, content_dir_);
    }

  private:
    template <int Low, int High>
    char RandomDigit() {
        static_assert(0 <= Low);
        static_assert(Low < High);
        static_assert(High < kBase32Digits.size());
        return kBase32Digits[absl::Uniform(absl::IntervalClosed, bitgen_, Low,
                                           High)];
    }

    std::filesystem::path SuggestDestinationFilename(int& depth) {
        // Generate a random destination directory name, and create it.
        std::filesystem::path destination = content_dir_;
        for (int i = 0; i < depth; ++i) {
            const char dirname[] = {RandomDigit<0, 15>(), RandomDigit<0, 31>()};
            destination /= std::string_view(dirname, std::size(dirname));
        }
        std::filesystem::create_directories(destination);

        // Generate a random filename.
        const char filename[] = {RandomDigit<16, 31>(), RandomDigit<0, 31>()};
        destination /= std::string_view(filename, std::size(filename));

        // Increment `depth`, in case we are called again.
        if (depth < kMaxContentDepth) {
            ++depth;
        }
        return destination;
    }

    // The maximum depth of the directory hierarchy we use when suggesting
    // filenames in the content directory.
    static constexpr int kMaxContentDepth = 4;

    const std::filesystem::path content_dir_;
    absl::BitGen bitgen_;
};

}  // namespace

std::unique_ptr<ContentStore> CreateDiskContentStore(
    const std::filesystem::path& content_dir) {
    return std::make_unique<DiskContentStore>(content_dir);
}

}  // namespace frz
