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
#include <git2.h>
#include <memory>
#include <optional>

#include "filesystem_util.hh"

namespace frz {
namespace {

class GitRepo final : public GitRepository {
  public:
    GitRepo(const std::filesystem::path repo_path)
        : repo_([&] {
              git_repository* repo;
              const int status = git_repository_open(&repo, repo_path.c_str());
              if (status != GIT_OK) {
                  throw GitError(status);
              }
              return repo;
          }()) {}

    ~GitRepo() override {
        if (index_ != nullptr) {
            git_index_free(index_);
        }
        git_repository_free(repo_);
    }

    std::optional<std::filesystem::path> WorkingDirectory() const override {
        const char* workdir = git_repository_workdir(repo_);
        if (workdir == nullptr) {
            return std::nullopt;
        } else {
            // libgit2 tends to give us paths with trailing directory
            // separators. We dance a bit to get rid of them.
            return (std::filesystem::path(workdir) / "dummy").parent_path();
        }
    }

    bool IsIgnored(const std::filesystem::path& path) const override {
        const std::optional<std::filesystem::path> workdir_root =
            WorkingDirectory();
        if (!workdir_root.has_value()) {
            return false;  // we have no git worktree
        }
        const std::optional<std::filesystem::path> workdir_path =
            RelativeSubtreePath(path, *workdir_root);
        if (!workdir_path.has_value()) {
            return false;  // `path` is not in the git worktree
        }
        if (*workdir_path == ".") {
            // `git_ignore_path_is_ignored` reports that "." is ignored, but
            // heeding that would make us ignore everything, since we don't
            // visit ignored directories.
            return false;
        }
        int ignored;
        const int status =
            git_ignore_path_is_ignored(&ignored, repo_, workdir_path->c_str());
        if (status == GIT_OK) {
            return ignored;
        } else {
            throw GitError(status);
        }
    }

    unsigned int Status(
        const std::filesystem::directory_entry& dent) const override {
        FRZ_ASSERT(dent.is_regular_file() || dent.is_symlink());
        const std::optional<std::filesystem::path> workdir_root =
            WorkingDirectory();
        FRZ_ASSERT(workdir_root.has_value());  // we must have a git worktree
        const std::optional<std::filesystem::path> workdir_path =
            RelativeSubtreePath(dent.path(), *workdir_root);
        FRZ_ASSERT(workdir_path.has_value());  // `dent` must be in the git
                                               // worktree
        unsigned int status_flags;
        const int status =
            git_status_file(&status_flags, repo_, workdir_path->c_str());
        if (status != GIT_OK) {
            throw GitError(status);
        }
        return status_flags;
    }

    void Add(const std::filesystem::directory_entry& dent) override {
        FRZ_ASSERT(dent.is_regular_file() || dent.is_symlink());
        if (index_ == nullptr) {
            const int status = git_repository_index(&index_, repo_);
            if (status != GIT_OK) {
                throw GitError(status);
            }
        }
        FRZ_ASSERT_NE(index_, nullptr);
        const std::optional<std::filesystem::path> workdir_root =
            WorkingDirectory();
        FRZ_ASSERT(workdir_root.has_value());  // we must have a git worktree
        const std::optional<std::filesystem::path> workdir_path =
            RelativeSubtreePath(dent.path(), *workdir_root);
        FRZ_ASSERT(workdir_path.has_value());  // `dent` must be in the git
                                               // worktree
        const int status = git_index_add_bypath(index_, workdir_path->c_str());
        if (status != GIT_OK) {
            throw GitError(status);
        }
    }

    void Save() override {
        if (index_ != nullptr) {
            const int status = git_index_write(index_);
            if (status != GIT_OK) {
                throw GitError(status);
            }
        }
    }

  private:
    const LibGit2Handle libgit2_handle_;
    git_repository* const repo_;
    git_index* index_ = nullptr;  // populated on demand
};

}  // namespace

std::optional<std::filesystem::path> GitRepository::Discover(
    const std::filesystem::path& path) {
    // `git_repository_discover` unfortunately follows symlinks before starting
    // its search. We don't want this, so if `path` is a symlink, we start the
    // search in its parent directory instead.
    const std::filesystem::path start_path =
        std::filesystem::is_symlink(path) ? path.parent_path() : path;
    LibGit2Handle libgit2_handle;
    GitBuf repo_path;
    const int status =
        git_repository_discover(repo_path.Get(), start_path.c_str(),
                                /*across_fs=*/false,
                                /*ceiling_dirs=*/nullptr);
    switch (status) {
        case GIT_OK:
            // libgit2 tends to give us paths with trailing directory
            // separators. We dance a bit to get rid of them.
            return (std::filesystem::path(repo_path.Get()->ptr) / "dummy")
                .parent_path();
        case GIT_ENOTFOUND:
            return std::nullopt;
        default:
            throw GitError(status);
    }
}

std::unique_ptr<GitRepository> GitRepository::Open(
    const std::filesystem::path& path) {
    return std::make_unique<GitRepo>(path);
}

}  // namespace frz
