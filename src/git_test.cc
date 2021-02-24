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

#include "git.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "filesystem_testing.hh"
#include "git_testing.hh"

namespace frz {
namespace {

using ::testing::ElementsAre;

TEST(TestGit, IgnoreNothingWithoutRepo) {
    TempDir d;
    const std::unique_ptr<Git> git = Git::Create();
    EXPECT_FALSE(git->IsIgnored(d.Path() / ".git"));
    EXPECT_FALSE(git->IsIgnored(d.Path() / "bar.foo"));
}

TEST(TestGit, IgnoreDotGit) {
    TempDir d;
    CreateGitRepository(d.Path());
    const std::unique_ptr<Git> git = Git::Create();
    EXPECT_TRUE(git->IsIgnored(d.Path() / ".git"));
    EXPECT_FALSE(git->IsIgnored(d.Path() / "bar.foo"));
}

TEST(TestGit, CustomIgnoreRule) {
    TempDir d;
    CreateGitRepository(d.Path());
    const std::unique_ptr<Git> git = Git::Create();
    EXPECT_FALSE(git->IsIgnored(d.Path() / "bar.foo"));
    d.File(".gitignore", "*.foo\n");
    EXPECT_TRUE(git->IsIgnored(d.Path() / "bar.foo"));
}

TEST(TestGit, AddFile) {
    TempDir d;
    CreateGitRepository(d.Path());
    d.File("file1", "abc");
    EXPECT_THAT(d.Path() / "file1", GitStatus(ElementsAre("worktree_new")));
    std::unique_ptr<Git> git = Git::Create();
    git->Add(d.Path() / "file1");
    EXPECT_THAT(d.Path() / "file1", GitStatus(ElementsAre("worktree_new")));
    git->Save();
    EXPECT_THAT(d.Path() / "file1", GitStatus(ElementsAre("index_new")));
}

TEST(TestGit, AddSymlink) {
    TempDir d;
    CreateGitRepository(d.Path());
    d.Symlink("sym1", "foo/bar");
    EXPECT_THAT(d.Path() / "sym1", GitStatus(ElementsAre("worktree_new")));
    std::unique_ptr<Git> git = Git::Create();
    git->Add(d.Path() / "sym1");
    EXPECT_THAT(d.Path() / "sym1", GitStatus(ElementsAre("worktree_new")));
    git->Save();
    EXPECT_THAT(d.Path() / "sym1", GitStatus(ElementsAre("index_new")));
}

}  // namespace
}  // namespace frz
