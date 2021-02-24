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

#include <absl/container/flat_hash_map.h>
#include <memory>

#include "assert.hh"
#include "git_impl.hh"

namespace frz {
namespace {

class GitState final : public Git {
  public:
    bool IsIgnored(const std::filesystem::directory_entry& dent) override {
        GitRepository* git_repo = FindGitRepo(dent);
        return git_repo == nullptr ? false : git_repo->IsIgnored(dent.path());
    }

    void Add(const std::filesystem::directory_entry& dent) override {
        GitRepository* git_repo = FindGitRepo(dent);
        if (git_repo != nullptr) {
            git_repo->Add(dent);
        }
    }

    void Save() override {
        for (auto& [path, repo] : git_repos_by_repo_path_) {
            repo->Save();
        }
    }

  private:
    // Returns a pointer to the git repository that owns the given file or
    // directory, or null if no git repository owns it.
    GitRepository* FindGitRepo(const std::filesystem::directory_entry& dent) {
        const std::filesystem::path dir =
            dent.is_directory() ? dent.path() : dent.path().parent_path();
        auto [it, inserted] =
            git_repos_by_worktree_path_.try_emplace(dir.native(), nullptr);
        auto& [key, val] = *it;
        if (inserted) {
            // There wan't a mapping for `dir`, so `val` is now a reference to
            // a just-inserted null value.
            std::optional<std::filesystem::path> repo_path =
                GitRepository::Discover(dir);
            if (repo_path.has_value()) {
                // There's a git repo that owns `dir`. Look it up in
                // `git_repos_by_repo_path_`, inserting it if it isn't already
                // there.
                std::unique_ptr<GitRepository>& repo =
                    git_repos_by_repo_path_[repo_path->native()];
                if (repo == nullptr) {
                    repo = GitRepository::Open(*repo_path);
                }
                val = repo.get();
                FRZ_ASSERT_NE(val, nullptr);
            } else {
                // No git repo owns `dir`.
                FRZ_ASSERT_EQ(val, nullptr);
            }
        }
        return val;
    }

    // Map from directory name to the git repository that lives there. Each
    // repository is listed at most once.
    absl::flat_hash_map<std::filesystem::path::string_type,
                        std::unique_ptr<GitRepository>>
        git_repos_by_repo_path_;

    // Map from worktree directory to the git repository that owns that
    // directory (the pointer is to one of the values in
    // `git_repos_by_repo_path_`), or null if no git repository owns it. Since
    // a worktree can have many subdirectories, each git repo can be listed
    // many times.
    absl::flat_hash_map<std::filesystem::path::string_type, GitRepository*>
        git_repos_by_worktree_path_;
};

}  // namespace

std::unique_ptr<Git> Git::Create() {
    return std::make_unique<GitState>();
}

}  // namespace frz
