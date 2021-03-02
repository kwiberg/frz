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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ranges>
#include <string>
#include <vector>

#include "command.hh"
#include "filesystem_testing.hh"

namespace frz {
namespace {

using ::testing::Eq;
using ::testing::StartsWith;
using ::testing::StrEq;

TEST(TestCommandFill, NoRepository) {
    TempDir d;
    EXPECT_EQ(1, Command(d.Path(), {"fill"}));
}

TEST(TestCommandFill, EmptyRepository) {
    TempDir d;
    d.Dir(".frz");
    EXPECT_EQ(0, Command(d.Path(), {"fill"}));
}

TempDir CreateSmallTestRepo() {
    TempDir d;
    d.Dir(".frz");
    d.File("file1", "123");
    d.File("file2", "456");
    d.File("file3", "789");
    EXPECT_EQ(0, Command(d.Path(), {"add", "."}));
    return d;
}

TEST(TestCommandFill, SmallTestRepoHasNoMissingFiles) {
    TempDir d = CreateSmallTestRepo();
    EXPECT_EQ(0, Command(d.Path(), {"fill"}));
    EXPECT_THAT(d.Path() / "file1",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("123"))));
}

TEST(TestCommandFill, MissingContentFileIsNotDetected) {
    TempDir d = CreateSmallTestRepo();
    d.Remove(".frz/content");
    d.Dir(".frz/content");
    EXPECT_EQ(0, Command(d.Path(), {"fill"}));
}

TEST(TestCommandFill, WrongContentSizeIsNotDetected) {
    TempDir d = CreateSmallTestRepo();
    AddWritePermission(d.FollowSymlinks("file1").back());
    d.File("file1", "1234");  // Append one character.
    EXPECT_EQ(0, Command(d.Path(), {"fill"}));
}

TEST(TestCommandFill, ContentBitflipIsNotDetected) {
    TempDir d = CreateSmallTestRepo();
    AddWritePermission(d.FollowSymlinks("file1").back());
    d.File("file1", "1x3");  // Replace one character.
    EXPECT_EQ(0, Command(d.Path(), {"fill"}));
}

TEST(TestCommandFill, AddsMissingFrzSymlink) {
    TempDir d;
    d.Dir(".frz");
    d.File("sub/file1", "123");
    EXPECT_EQ(0, Command(d.Path(), {"add", "."}));
    EXPECT_THAT(d.Path() / "sub" / ".frz", IsSymlinkWhoseTarget(Eq("../.frz")));
    d.Remove("sub/.frz");
    EXPECT_THAT(d.Path() / "sub" / ".frz", IsNotFound());
    EXPECT_EQ(0, Command(d.Path(), {"fill"}));
    EXPECT_THAT(d.Path() / "sub" / ".frz", IsSymlinkWhoseTarget(Eq("../.frz")));
}

TEST(TestCommandFill, MovesFromUnusedContent) {
    TempDir d = CreateSmallTestRepo();
    d.Remove(".frz/content");
    d.Remove(".frz/blake3");
    EXPECT_EQ(1, Command(d.Path(), {"fill"}));
    d.File(".frz/unused-content/foo", "123");
    d.File(".frz/unused-content/bar", "456");
    d.File(".frz/unused-content/sub/baz", "789");
    EXPECT_EQ(0, Command(d.Path(), {"fill"}));
    EXPECT_THAT(d.Path() / ".frz/unused-content/foo", IsNotFound());
    EXPECT_THAT(d.Path() / "file1", ReadContents(StrEq("123")));
}

TEST(TestCommandFill, CopyFrom) {
    TempDir d = CreateSmallTestRepo();
    // Keep "file1", but remove the rest of its symlink chain.
    for (auto paths = d.FollowSymlinks("file1");
         const std::filesystem::path& p : paths | std::views::drop(1)) {
        std::filesystem::remove(p);
    }
    d.File("sub/foo", "123");  // Same content as the original file.
    EXPECT_EQ(0, Command(d.Path(), {"fill", "--copy-from", "sub"}));
    EXPECT_THAT(d.Path() / "file1", ReadContents(StrEq("123")));
    EXPECT_THAT(d.Path() / "sub/foo", ReadContents(StrEq("123")));
}

TEST(TestCommandFill, MoveFrom) {
    TempDir d = CreateSmallTestRepo();
    // Keep "file1", but remove the rest of its symlink chain.
    for (auto paths = d.FollowSymlinks("file1");
         const std::filesystem::path& p : paths | std::views::drop(1)) {
        std::filesystem::remove(p);
    }
    d.File("sub/foo", "123");  // Same content as the original file.
    EXPECT_EQ(0, Command(d.Path(), {"fill", "--move-from", "sub"}));
    EXPECT_THAT(d.Path() / "file1", ReadContents(StrEq("123")));
    EXPECT_THAT(d.Path() / "sub/foo", IsNotFound());
}

TEST(TestCommandFill, ContentSourcesAreOrdered) {
    TempDir d = CreateSmallTestRepo();
    d.Remove(".frz/content");
    d.Remove(".frz/blake3");
    d.File("sub1/1", "123");
    d.File("sub2/x", "123");
    d.File("sub2/y", "456");
    d.File("sub3/a", "123");
    d.File("sub3/b", "456");
    d.File("sub3/c", "789");
    EXPECT_EQ(0,
              Command(d.Path(), {"fill", "--move-from", "sub1", "--copy-from",
                                 "sub2", "--move-from", "sub3"}));

    // We have moved the "123" content from sub1 (highest prio source), copied
    // the "456" content from sub2 (medium prio source), and moved the "789"
    // content from sub3 (lowest prio source).
    EXPECT_THAT(d.Path() / "sub1/1", IsNotFound());
    EXPECT_THAT(d.Path() / "sub2/x", ReadContents(StrEq("123")));
    EXPECT_THAT(d.Path() / "sub2/y", ReadContents(StrEq("456")));
    EXPECT_THAT(d.Path() / "sub3/a", ReadContents(StrEq("123")));
    EXPECT_THAT(d.Path() / "sub3/b", ReadContents(StrEq("456")));
    EXPECT_THAT(d.Path() / "sub3/c", IsNotFound());
}

TEST(TestCommandFill, CopyFromUnreadable) {
    // Create the small test repo, and delete its contents.
    TempDir d = CreateSmallTestRepo();
    d.Remove(".frz/blake3");
    d.Remove(".frz/content");

    // Make a directory whose file has the same contents...
    d.File("sub/fileA", "123");
    d.File("sub/fileB", "456");
    d.File("sub/fileC", "789");

    // ...but make one of the files unreadable.
    std::filesystem::permissions(d.Path() / "sub/fileB",
                                 std::filesystem::perms::owner_read |
                                     std::filesystem::perms::group_read |
                                     std::filesystem::perms::others_read,
                                 std::filesystem::perm_options::remove);

    // We expect `frz fill` to report that it failed...
    EXPECT_EQ(1, Command(d.Path(), {"fill", "--copy-from", "sub"}));

    // ...but it should have been able to fill in the contents of the two
    // readable files.
    EXPECT_THAT(d.Path() / "file1", ReadContents(StrEq("123")));
    EXPECT_THAT(d.Path() / "file3", ReadContents(StrEq("789")));
}

TEST(TestCommandFill, WriteFailure) {
    TempDir d = CreateSmallTestRepo();

    // Keep "file1", but remove the rest of its symlink chain.
    for (auto paths = d.FollowSymlinks("file1");
         const std::filesystem::path& p : paths | std::views::drop(1)) {
        std::filesystem::remove(p);
    }
    d.File("sub/foo", "123");  // Same content as the original file.

    // Write protect `.frz/content/`; this will cause a failure when we try to
    // copy the new content file in. This is an unlikely problem in practice,
    // but it causes a failure in the same place as an out-of-disk-space error
    // would.
    std::filesystem::permissions(d.Path() / ".frz/content",
                                 std::filesystem::perms::owner_write |
                                     std::filesystem::perms::group_write |
                                     std::filesystem::perms::others_write,
                                 std::filesystem::perm_options::remove);

    // We expect this to fail because of the write protection, but it should do
    // so gracefully!
    EXPECT_EQ(1, Command(d.Path(), {"fill", "--copy-from", "sub"}));
}

TEST(TestCommandFill, ContentSourcesIgnoreSymlinks) {
    TempDir d = CreateSmallTestRepo();

    // Keep "file1", but remove the rest of its symlink chain.
    for (auto paths = d.FollowSymlinks("file1");
         const std::filesystem::path& p : paths | std::views::drop(1)) {
        std::filesystem::remove(p);
    }
    d.File("sub1/foo", "123");  // Same content as the original file.
    d.Symlink("sub2/foo", "../sub1/foo");

    // Fails because we ignore symlinks in content source trees.
    EXPECT_EQ(1, Command(d.Path(), {"fill", "--copy-from", "sub2"}));

    // Succeeds, because sub1/foo is the real file.
    EXPECT_EQ(0, Command(d.Path(), {"fill", "--copy-from", "sub1"}));
}

}  // namespace
}  // namespace frz
