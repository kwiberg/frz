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

#include "git_testing.hh"

#include <absl/strings/str_join.h>
#include <filesystem>
#include <git2.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

#include "git_impl.hh"

namespace frz {
namespace {

// Translate a bitmask of `git_status_t` flags (see
// https://github.com/libgit2/libgit2/blob/main/include/git2/status.h) to a
// vector of human-readable strings.
std::vector<std::string_view> GitStatusToStrings(unsigned int status) {
    struct Flag {
        unsigned int flag;
        std::string_view desc;
    };
    static constexpr Flag flags[] = {
        {GIT_STATUS_INDEX_NEW, "index_new"},
        {GIT_STATUS_INDEX_MODIFIED, "index_modified"},
        {GIT_STATUS_INDEX_DELETED, "index_deleted"},
        {GIT_STATUS_INDEX_RENAMED, "index_renamed"},
        {GIT_STATUS_INDEX_TYPECHANGE, "index_typechange"},
        {GIT_STATUS_WT_NEW, "worktree_new"},
        {GIT_STATUS_WT_MODIFIED, "worktree_modified"},
        {GIT_STATUS_WT_DELETED, "worktree_deleted"},
        {GIT_STATUS_WT_TYPECHANGE, "worktree_typechange"},
        {GIT_STATUS_WT_RENAMED, "worktree_renamed"},
        {GIT_STATUS_WT_UNREADABLE, "worktree_unreadable"},
        {GIT_STATUS_IGNORED, "ignored"},
        {GIT_STATUS_CONFLICTED, "conflicted"},
    };
    std::vector<std::string_view> s;
    for (const auto& [flag, desc] : flags) {
        if ((status & flag) != 0) {
            s.push_back(desc);
        }
    }
    return s;
}

class GitStatusMatcher final {
  public:
    using is_gtest_matcher = void;

    GitStatusMatcher(
        testing::Matcher<std::vector<std::string_view>> status_matcher)
        : status_matcher_(std::move(status_matcher)) {}

    bool MatchAndExplain(const std::filesystem::path& path,
                         std::ostream* out) const {
        std::optional<std::filesystem::path> repo_path =
            GitRepository::Discover(path);
        if (!repo_path.has_value()) {
            if (out != nullptr) {
                *out << "is a filesystem path that is not under git control";
            }
            return false;
        }
        std::unique_ptr<GitRepository> repo = GitRepository::Open(*repo_path);
        std::vector<std::string_view> status =
            GitStatusToStrings(repo->Status(path));
        if (status_matcher_.Matches(status)) {
            return true;
        } else {
            if (out != nullptr) {
                *out << "is a filesystem path whose git status is "
                     << absl::StrJoin(status, "+");
            }
            return false;
        }
    }

    void DescribeTo(std::ostream* out) const { Desc("is", out); }
    void DescribeNegationTo(std::ostream* out) const { Desc("is not", out); }

  private:
    void Desc(std::string_view prefix, std::ostream* out) const {
        *out << prefix << " a filesystem path whose git status ";
        status_matcher_.DescribeTo(out);
    }

    testing::Matcher<std::vector<std::string_view>> status_matcher_;
};

}  // namespace

testing::Matcher<std::filesystem::path> GitStatus(
    testing::Matcher<std::vector<std::string_view>> status_matcher) {
    return GitStatusMatcher(std::move(status_matcher));
}

void CreateGitRepository(const std::filesystem::path& dir) {
    LibGit2Handle libgit2_handle;
    const int status = git_repository_init(
        /*out=*/nullptr,
        /*path=*/dir.c_str(),
        /*is_bare=*/false);
    if (status != GIT_OK) {
        throw GitError(status);
    }
}

}  // namespace frz
