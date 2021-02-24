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
#include <string>
#include <vector>

#include "command.hh"
#include "filesystem_testing.hh"
#include "filesystem_util.hh"

namespace frz {
namespace {

using ::testing::Eq;
using ::testing::StartsWith;
using ::testing::StrEq;

class TestCommandRepair : public testing::TestWithParam<bool> {
  public:
    bool IsFast() const { return GetParam(); }
    int RunRepair(const std::filesystem::path& working_dir,
                  std::vector<std::string> args = {}) {
        if (IsFast()) {
            args.insert(args.begin(), "--fast");
        }
        args.insert(args.begin(), "repair");
        return Command(working_dir, args);
    }
};
INSTANTIATE_TEST_SUITE_P(, TestCommandRepair, testing::Bool(),
                         [](const auto& info) {
                             return info.param ? "fast" : "nofast";
                         });

TEST_P(TestCommandRepair, NoRepository) {
    TempDir d;
    EXPECT_EQ(1, RunRepair(d.Path()));
}

TEST_P(TestCommandRepair, EmptyRepository) {
    TempDir d;
    d.Dir(".frz");
    EXPECT_EQ(0, RunRepair(d.Path()));
}

TEST_P(TestCommandRepair, IndexDirIsSymlink) {
    TempDir d;
    d.Dir(".frz/blake2");
    d.Symlink(".frz/blake3", "blake2");
    EXPECT_EQ(1, RunRepair(d.Path()));
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

TEST_P(TestCommandRepair, SmallTestRepoHasNoErrors) {
    TempDir d = CreateSmallTestRepo();
    EXPECT_EQ(0, RunRepair(d.Path()));
    EXPECT_THAT(d.Path() / "file1",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("123"))));
}

TEST_P(TestCommandRepair, MissingContentFileIsDetected) {
    TempDir d = CreateSmallTestRepo();
    d.Remove(".frz/content");
    d.Dir(".frz/content");
    EXPECT_EQ(1, RunRepair(d.Path()));
}

TEST_P(TestCommandRepair, WrongContentSizeIsDetected) {
    TempDir d = CreateSmallTestRepo();
    AddWritePermission(d.FollowSymlinks("file1").back());
    d.File("file1", "1234");  // Append one character.
    EXPECT_EQ(1, RunRepair(d.Path()));
}

TEST_P(TestCommandRepair, ContentBitflipIsDetected) {
    TempDir d = CreateSmallTestRepo();
    AddWritePermission(d.FollowSymlinks("file1").back());
    d.File("file1", "1x3");  // Replace one character.
    if (IsFast()) {
        // With --fast, we can't detect a content modification that doesn't
        // change the file size.
        EXPECT_EQ(0, RunRepair(d.Path()));
    } else {
        // Without --fast, we can detect it.
        EXPECT_EQ(1, RunRepair(d.Path()));
    }
}

TEST_P(TestCommandRepair, ContentFilePermissions) {
    TempDir d = CreateSmallTestRepo();
    EXPECT_TRUE(IsReadonly(
        std::filesystem::symlink_status(d.FollowSymlinks("file1").back())));
    AddWritePermission(d.FollowSymlinks("file1").back());
    EXPECT_FALSE(IsReadonly(
        std::filesystem::symlink_status(d.FollowSymlinks("file1").back())));
    EXPECT_EQ(0, RunRepair(d.Path()));
    EXPECT_TRUE(IsReadonly(
        std::filesystem::symlink_status(d.FollowSymlinks("file1").back())));
}

TEST_P(TestCommandRepair, AddsMissingFrzSymlink) {
    TempDir d;
    d.Dir(".frz");
    d.File("sub/file1", "123");
    EXPECT_EQ(0, Command(d.Path(), {"add", "."}));
    EXPECT_THAT(d.Path() / "sub" / ".frz", IsSymlinkWhoseTarget(Eq("../.frz")));
    d.Remove("sub/.frz");
    EXPECT_THAT(d.Path() / "sub" / ".frz", IsNotFound());
    EXPECT_EQ(0, RunRepair(d.Path()));
    EXPECT_THAT(d.Path() / "sub" / ".frz", IsSymlinkWhoseTarget(Eq("../.frz")));
}

TEST_P(TestCommandRepair, MovesFromUnusedContent) {
    TempDir d = CreateSmallTestRepo();
    d.Remove(".frz/content");
    EXPECT_EQ(1, RunRepair(d.Path()));
    d.File(".frz/unused-content/foo", "123");
    d.File(".frz/unused-content/bar", "456");
    d.File(".frz/unused-content/sub/baz", "789");
    EXPECT_EQ(0, RunRepair(d.Path()));
    EXPECT_THAT(d.Path() / ".frz/unused-content/foo", IsNotFound());
    EXPECT_THAT(d.Path() / "file1", ReadContents(StrEq("123")));
}

TEST_P(TestCommandRepair, CopyFrom) {
    TempDir d = CreateSmallTestRepo();
    AddWritePermission(d.FollowSymlinks("file1").back());
    d.File("file1", "1x3");    // Replace one character.
    d.File("sub/foo", "123");  // Same content as the original file.
    EXPECT_EQ(0, RunRepair(d.Path(), {"--copy-from", "sub"}));
    if (IsFast()) {
        // With --fast, we couldn't detect the content modification, so we
        // didn't repair it.
        EXPECT_THAT(d.Path() / "file1", ReadContents(StrEq("1x3")));
    } else {
        // Without --fast, we could detect it, and did repair it.
        EXPECT_THAT(d.Path() / "file1", ReadContents(StrEq("123")));
    }
    EXPECT_THAT(d.Path() / "sub/foo", ReadContents(StrEq("123")));
}

TEST_P(TestCommandRepair, MoveFrom) {
    TempDir d = CreateSmallTestRepo();
    AddWritePermission(d.FollowSymlinks("file1").back());
    d.File("file1", "1234");   // Append one character.
    d.File("sub/foo", "123");  // Same content as the original file.
    EXPECT_EQ(0, RunRepair(d.Path(), {"--move-from", "sub"}));
    EXPECT_THAT(d.Path() / "file1", ReadContents(StrEq("123")));
    EXPECT_THAT(d.Path() / "sub/foo", IsNotFound());
}

TEST_P(TestCommandRepair, ContentSourcesAreOrdered) {
    TempDir d = CreateSmallTestRepo();
    d.Remove(".frz/content");
    d.File("sub1/1", "123");
    d.File("sub2/x", "123");
    d.File("sub2/y", "456");
    d.File("sub3/a", "123");
    d.File("sub3/b", "456");
    d.File("sub3/c", "789");
    EXPECT_EQ(0, RunRepair(d.Path(), {"--move-from", "sub1", "--copy-from",
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

}  // namespace
}  // namespace frz
