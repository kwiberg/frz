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

#include "git_impl.hh"

#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>

#include "filesystem_testing.hh"
#include "git_testing.hh"

namespace frz {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Optional;

TEST(TestGitImpl, DiscoverNoRepo) {
    TempDir d;
    EXPECT_THAT(GitRepository::Discover(d.Path()), Eq(std::nullopt));
}

TEST(TestGitImpl, DiscoverRepoInSameDirectory) {
    TempDir d;
    CreateGitRepository(d.Path());
    EXPECT_THAT(GitRepository::Discover(d.Path()), Optional(d.Path() / ".git"));
}

TEST(TestGitImpl, DiscoverRepoInSubDirectory) {
    TempDir d;
    d.Dir("sub/sub");
    CreateGitRepository(d.Path());
    EXPECT_THAT(GitRepository::Discover(d.Path() / "sub" / "sub"),
                Optional(d.Path() / ".git"));
}

TEST(TestGitImpl, IgnoreDotGit) {
    TempDir d;
    CreateGitRepository(d.Path());
    EXPECT_THAT(
        GitRepository::Open(d.Path()),
        Pointee(ResultOf(
            [&](auto& repo) { return repo.IsIgnored(d.Path() / ".git"); },
            IsTrue())));
}

TEST(TestGitImpl, CustomIgnoreRule) {
    TempDir d;
    CreateGitRepository(d.Path());
    std::unique_ptr<GitRepository> repo = GitRepository::Open(d.Path());
    EXPECT_FALSE(repo->IsIgnored(d.Path() / "bar.foo"));
    d.File(".gitignore", "*.foo\n");
    EXPECT_TRUE(repo->IsIgnored(d.Path() / "bar.foo"));
}

TEST(TestGitImpl, AddFile) {
    TempDir d;
    CreateGitRepository(d.Path());
    d.File("file1", "abc");
    EXPECT_THAT(d.Path() / "file1", GitStatus(ElementsAre("worktree_new")));
    std::unique_ptr<GitRepository> repo = GitRepository::Open(d.Path());
    repo->Add(d.Path() / "file1");
    EXPECT_THAT(d.Path() / "file1", GitStatus(ElementsAre("worktree_new")));
    repo->Save();
    EXPECT_THAT(d.Path() / "file1", GitStatus(ElementsAre("index_new")));
}

TEST(TestGitImpl, AddSymlink) {
    TempDir d;
    CreateGitRepository(d.Path());
    d.Symlink("sym1", "foo/bar");
    EXPECT_THAT(d.Path() / "sym1", GitStatus(ElementsAre("worktree_new")));
    std::unique_ptr<GitRepository> repo = GitRepository::Open(d.Path());
    repo->Add(d.Path() / "sym1");
    EXPECT_THAT(d.Path() / "sym1", GitStatus(ElementsAre("worktree_new")));
    repo->Save();
    EXPECT_THAT(d.Path() / "sym1", GitStatus(ElementsAre("index_new")));
}

}  // namespace
}  // namespace frz
