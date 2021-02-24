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

#include <absl/strings/str_format.h>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <vector>

#include "command.hh"
#include "filesystem_testing.hh"
#include "git_testing.hh"

namespace frz {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrEq;

// Fixture for tests that run with+without git, and with+without using "." as
// the `frz add` path arguent.
class TestCommandAdd1 : public testing::TestWithParam<std::tuple<bool, bool>> {
  public:
    bool UseGit() const { return std::get<0>(GetParam()); }
    bool AddWithDot() const { return std::get<1>(GetParam()); }
};
INSTANTIATE_TEST_SUITE_P(, TestCommandAdd1,
                         testing::Combine(/*use_git=*/testing::Bool(),
                                          /*add_with_dot=*/testing::Bool()),
                         [](const auto& info) {
                             return absl::StrFormat("git%d_dot%d",
                                                    std::get<0>(info.param),
                                                    std::get<1>(info.param));
                         });

TEST_P(TestCommandAdd1, OneFileInRoot) {
    TempDir d;
    d.Dir(".frz");
    if (UseGit()) {
        CreateGitRepository(d.Path());
    }
    d.File("foo", "bar");

    EXPECT_THAT(d.Path() / "foo",
                AllOf(IsRegularFile(), ReadContents(StrEq("bar")),
                      UseGit() ? GitStatus(ElementsAre("worktree_new"))
                               : Not(GitStatus(_))));

    EXPECT_EQ(0, Command(d.Path(), {"add", AddWithDot() ? "." : "foo"}));

    {
        std::ofstream out(d.Path() / "foo", std::ios::binary | std::ios::trunc);
        out << "try overwriting the added file";
        out.close();
        EXPECT_TRUE(out.fail());
    }

    // Since the content file was write protected, we expect it to not have
    // been overwritten.
    EXPECT_THAT(d.Path() / "foo",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("bar")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));
}

TEST_P(TestCommandAdd1, OneFileInSubdir) {
    TempDir d;
    d.Dir(".frz");
    if (UseGit()) {
        CreateGitRepository(d.Path());
    }
    d.File("sub/dir/foo", "gg");

    EXPECT_THAT(d.Path() / "sub/dir/foo",
                AllOf(IsRegularFile(), ReadContents(StrEq("gg")),
                      UseGit() ? GitStatus(ElementsAre("worktree_new"))
                               : Not(GitStatus(_))));

    EXPECT_EQ(0,
              Command(d.Path(), {"add", AddWithDot() ? "." : "sub/dir/foo"}));

    EXPECT_THAT(d.Path() / "sub/dir/.frz",
                IsSymlinkWhoseTarget(StrEq("../../.frz")));
    EXPECT_THAT(d.Path() / "sub/dir/foo",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("gg")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));
}

TEST_P(TestCommandAdd1, DirectoryTree) {
    TempDir d;
    d.Dir(".frz");
    if (UseGit()) {
        CreateGitRepository(d.Path());
    }
    d.File("sub/dir/foo", "gg");
    d.File("sub/dir/bar", "bb");
    d.File("sub/sume", "fff");

    EXPECT_EQ(0, Command(d.Path(), {"add", AddWithDot() ? "." : "sub"}));

    EXPECT_THAT(d.Path() / "sub/dir/.frz",
                IsSymlinkWhoseTarget(StrEq("../../.frz")));
    EXPECT_THAT(d.Path() / "sub/.frz", IsSymlinkWhoseTarget(StrEq("../.frz")));
    EXPECT_THAT(d.Path() / "sub/dir/foo",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("gg")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));
    EXPECT_THAT(d.Path() / "sub/dir/bar",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("bb")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));
    EXPECT_THAT(d.Path() / "sub/sume",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("fff")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));
}

TEST_P(TestCommandAdd1, Duplicates) {
    TempDir d;
    d.Dir(".frz");
    if (UseGit()) {
        CreateGitRepository(d.Path());
    }
    d.File("sub/dir/bar", "12");
    d.File("sub/sume", "12");
    d.File("sub/marine", "12");

    EXPECT_EQ(0, Command(d.Path(), {"add", "sub/dir", "sub/sume"}));

    EXPECT_THAT(d.Path() / "sub/dir/bar",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("12")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));
    EXPECT_THAT(d.Path() / "sub/sume",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("12")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));
    EXPECT_THAT(d.Path() / "sub/marine",
                AllOf(IsRegularFile(), ReadContents(StrEq("12")),
                      UseGit() ? GitStatus(ElementsAre("worktree_new"))
                               : Not(GitStatus(_))));
    EXPECT_THAT(RecursiveListDirectory(d.Path() / ".frz/content"),
                ElementsAre(AllOf(IsRegularFile(), ReadContents(StrEq("12")))));
    EXPECT_THAT(RecursiveListDirectory(d.Path() / ".frz/unused-content"),
                ElementsAre(AllOf(IsRegularFile(), ReadContents(StrEq("12")))));

    EXPECT_EQ(0, Command(d.Path(), {"add", AddWithDot() ? "." : "sub"}));

    EXPECT_THAT(d.Path() / "sub/marine",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("12")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));
    EXPECT_THAT(RecursiveListDirectory(d.Path() / ".frz/content"),
                ElementsAre(AllOf(IsRegularFile(), ReadContents(StrEq("12")))));
    EXPECT_THAT(RecursiveListDirectory(d.Path() / ".frz/unused-content"),
                AllOf(SizeIs(2),
                      Each(AllOf(IsRegularFile(), ReadContents(StrEq("12"))))));
}

TEST_P(TestCommandAdd1, NoFrzDirectory) {
    TempDir d;
    if (UseGit()) {
        CreateGitRepository(d.Path());
    }
    d.File("x", "y");

    EXPECT_NE(0, Command(d.Path(), {"add", AddWithDot() ? "." : "x"}));

    EXPECT_THAT(d.Path() / "x",
                AllOf(IsRegularFile(), ReadContents(StrEq("y")),
                      UseGit() ? GitStatus(ElementsAre("worktree_new"))
                               : Not(GitStatus(_))));
}

TEST_P(TestCommandAdd1, FrzFileInSubdir) {
    TempDir d;
    d.Dir(".frz");
    if (UseGit()) {
        CreateGitRepository(d.Path());
    }
    d.File("sub/x", "qq");
    d.File("sub/.frz", "q");

    EXPECT_NE(0, Command(d.Path(), {"add", AddWithDot() ? "." : "sub"}));

    EXPECT_THAT(d.Path() / "sub/x",
                AllOf(IsRegularFile(), ReadContents(StrEq("qq")),
                      UseGit() ? GitStatus(ElementsAre("worktree_new"))
                               : Not(GitStatus(_))));
}

TEST_P(TestCommandAdd1, GitIgnoreGlob) {
    TempDir d;
    d.Dir(".frz");
    if (UseGit()) {
        CreateGitRepository(d.Path());
    }
    d.File(".gitignore", "*.foo\n");
    d.File("foo.bar", "1");
    d.File("bar.foo", "2");

    EXPECT_EQ(0, AddWithDot()
                     ? Command(d.Path(), {"add", "."})
                     : Command(d.Path(), {"add", "foo.bar", "bar.foo"}));

    EXPECT_THAT(d.Path() / "foo.bar",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("1")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));
    EXPECT_THAT(
        d.Path() / "bar.foo",
        AllOf(
            UseGit() ? IsRegularFile()
                     : IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
            ReadContents(StrEq("2")),
            UseGit() ? GitStatus(ElementsAre("ignored")) : Not(GitStatus(_))));
}

// Fixture for tests that run with+without git.
class TestCommandAdd2 : public testing::TestWithParam<bool> {
  public:
    bool UseGit() const { return GetParam(); }
};
INSTANTIATE_TEST_SUITE_P(, TestCommandAdd2, testing::Bool(),
                         [](const auto& info) {
                             return absl::StrFormat("git%d", info.param);
                         });

TEST_P(TestCommandAdd2, FrzDirectoryIsIgnored) {
    TempDir d;
    d.Dir(".frz");
    if (UseGit()) {
        CreateGitRepository(d.Path());
    }
    d.File("foo", "bar");

    EXPECT_EQ(0, Command(d.Path(), {"add", "."}));

    EXPECT_THAT(d.Path() / "foo",
                AllOf(IsSymlinkWhoseTarget(StartsWith(".frz/blake3/")),
                      ReadContents(StrEq("bar")),
                      UseGit() ? GitStatus(ElementsAre("index_new"))
                               : Not(GitStatus(_))));

    EXPECT_EQ(0, Command(d.Path(), {"add", "."}));

    EXPECT_THAT(
        RecursiveListDirectory(d.Path() / ".frz/content"),
        ElementsAre(AllOf(IsRegularFile(), ReadContents(StrEq("bar")))));
}

}  // namespace
}  // namespace frz
