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

// Googletest matcher that tests if the value is a regular file.
testing::Matcher<std::filesystem::path> IsRegularFile();

// Googletest matcher that tests if the value is a path that doesn't exist.
testing::Matcher<std::filesystem::path> IsNotFound();

// Googletest matcher that tests if the value is a symlink, whose target path
// satisfies `target_matcher`.
testing::Matcher<std::filesystem::path> IsSymlinkWhoseTarget(
    testing::Matcher<std::filesystem::path> target_matcher);

// Googletest matcher that tests if the value is a file whose contents
// satisfies `content_matcher`.
testing::Matcher<std::filesystem::path> ReadContents(
    testing::Matcher<std::string> content_matcher);

// Traverse the directory tree rooted at `dir`, and put all non-directory
// things (regular files, symlinks, etc.) in a vector.
std::vector<std::filesystem::path> RecursiveListDirectory(
    const std::filesystem::path& dir);

// Make the given file writable.
inline void AddWritePermission(const std::filesystem::path path) {
    std::filesystem::permissions(path, std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::add |
                                     std::filesystem::perm_options::nofollow);
}

// RAII class that creates a temporary directory in the constructor, and
// deletes it again (along with its contents) in the destructor.
class TempDir final {
  public:
    // Create a new temporary directory.
    TempDir() : path_(CreateTempDir()) {}

    // Not copyable. Move constructible, but not move assignable.
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

    // Return the path to the temporary directory.
    const std::filesystem::path& Path() const { return path_; }

    // Create a new (empty) directory under the temporary directory. If `dir`
    // has multiple path components, the intermediate directories will also be
    // created if they don't already exist.
    void Dir(const std::filesystem::path& dir) {
        std::filesystem::create_directories(path_ / dir);
    }

    // Create a new regular file with the given contents under the temporary
    // directory. If `file` has multiple path components, the intermediate
    // directories will also be created if they don't already exist.
    void File(const std::filesystem::path& file, std::string_view contents) {
        std::filesystem::create_directories(path_ / file.parent_path());
        std::ofstream out(path_ / file, std::ios::binary | std::ios::trunc);
        out << contents;
        out.close();
        EXPECT_FALSE(out.fail());
    }

    // Create a new symlink with the given target under the temporary
    // directory. As usual with symlinks, the target path is relative to the
    // directory in which the symlink is located. If `link` has multiple path
    // components, the intermediate directories will also be created if they
    // don't already exist.
    void Symlink(const std::filesystem::path& link,
                 const std::filesystem::path& target) {
        std::filesystem::create_directories(path_ / link.parent_path());
        std::filesystem::create_symlink(target, path_ / link);
    }

    // Recursively remove the given directory.
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
    // Create a temporary directory and return its path. It will be a
    // subdirectory of std::filesystem::temp_directory_path().
    static std::filesystem::path CreateTempDir();

    // The path to the temporary directory that we own.
    std::filesystem::path path_;
};

}  // namespace frz
