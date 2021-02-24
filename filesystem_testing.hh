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

#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace frz {

testing::Matcher<std::filesystem::path> IsRegularFile();

testing::Matcher<std::filesystem::path> IsNotFound();

testing::Matcher<std::filesystem::path> IsSymlinkWhoseTarget(
    testing::Matcher<std::filesystem::path> target_matcher);

testing::Matcher<std::filesystem::path> ReadContents(
    testing::Matcher<std::string> content_matcher);

std::vector<std::filesystem::path> RecursiveListDirectory(
    const std::filesystem::path& dir);

inline void AddWritePermission(const std::filesystem::path path) {
    std::filesystem::permissions(path, std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::add |
                                     std::filesystem::perm_options::nofollow);
}

class TempDir final {
  public:
    TempDir() : path_(CreateTempDir()) {}
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&& other) {
        using std::swap;
        swap(path_, other.path_);
    }
    TempDir& operator=(TempDir&&) = delete;
    ~TempDir() {
        if (!path_.empty()) {
            std::filesystem::remove_all(path_);
        }
    }

    const std::filesystem::path& Path() const { return path_; }

    void Dir(const std::filesystem::path& dir) {
        std::filesystem::create_directories(path_ / dir);
    }

    void File(const std::filesystem::path& file, std::string_view contents) {
        std::filesystem::create_directories(path_ / file.parent_path());
        std::ofstream out(path_ / file, std::ios::binary | std::ios::trunc);
        out << contents;
        out.close();
        EXPECT_FALSE(out.fail());
    }

    void Symlink(const std::filesystem::path& link,
                 const std::filesystem::path& target) {
        std::filesystem::create_directories(path_ / link.parent_path());
        std::filesystem::create_symlink(target, path_ / link);
    }

    void Remove(const std::filesystem::path& dir) {
        std::filesystem::remove_all(path_ / dir);
    }

    // Return a vector with the given symlink, the link it's pointing to, the
    // link that link is pointing to, etc., ending with the first path that
    // isn't a symlink. (As a result, if `link` isn't a symlink, the return
    // value is {link}.) The returned paths are lexically normalized.
    std::vector<std::filesystem::path> FollowSymlinks(
        const std::filesystem::path& link) const {
        std::vector<std::filesystem::path> targets = {
            (path_ / link).lexically_normal()};
        while (std::filesystem::is_symlink(targets.back())) {
            const std::filesystem::path& p = targets.back();
            targets.push_back(
                (p.parent_path() / std::filesystem::read_symlink(p))
                    .lexically_normal());
        }
        return targets;
    }

  private:
    // Create a temp directory and return its path.
    static std::filesystem::path CreateTempDir();

    std::filesystem::path path_;
};

}  // namespace frz
