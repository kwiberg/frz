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

#ifndef FRZ_GIT_IMPL_HH_
#define FRZ_GIT_IMPL_HH_

#include <filesystem>
#include <git2.h>

#include "assert.hh"
#include "exceptions.hh"

namespace frz {

// Instances of this class keep libgit2 open while they exist.
class LibGit2Handle final {
  public:
    LibGit2Handle() { git_libgit2_init(); }
    LibGit2Handle(const LibGit2Handle&) = default;
    LibGit2Handle& operator=(const LibGit2Handle&) = default;
    ~LibGit2Handle() { git_libgit2_shutdown(); }
};

// RAII wrapper for `git_buf`.
class GitBuf final {
  public:
    GitBuf() = default;
    GitBuf(const GitBuf&) = delete;
    GitBuf& operator=(const GitBuf&) = delete;
    ~GitBuf() { git_buf_dispose(&buf_); }
    git_buf* Get() { return &buf_; }

  private:
    git_buf buf_{.ptr = nullptr, .asize = 0, .size = 0};
};

// Immediately after a libgit2 function has returned a non-OK status, call this
// function to get an appropriate Error() instance that you can throw.
inline Error GitError(int status) {
    FRZ_ASSERT_NE(status, GIT_OK);
    const git_error* const e = git_error_last();
    return e == nullptr ? Error("libgit2 error %d", status) : Error(e->message);
}

// Represents one git repository.
class GitRepository {
  public:
    // Return the path of the git repository that owns `path`. If no git
    // repository owns `path`, return nullopt.
    static std::optional<std::filesystem::path> Discover(
        const std::filesystem::path& path);

    // Open the git repository located at `path`.
    static std::unique_ptr<GitRepository> Open(
        const std::filesystem::path& path);

    virtual ~GitRepository() = default;

    virtual std::optional<std::filesystem::path> WorkingDirectory() const = 0;

    virtual bool IsIgnored(const std::filesystem::path& path) const = 0;

    // Given a file in the repository's worktree, return a bitmask of
    // `git_status_t` flags (see
    // https://github.com/libgit2/libgit2/blob/main/include/git2/status.h).
    virtual unsigned int Status(
        const std::filesystem::directory_entry& dent) const = 0;
    unsigned int Status(const std::filesystem::path& path) const {
        return Status(std::filesystem::directory_entry(path));
    }

    virtual void Add(const std::filesystem::directory_entry& dent) = 0;
    void Add(const std::filesystem::path& path) {
        Add(std::filesystem::directory_entry(path));
    }

    // Save in-memory changes to disk.
    virtual void Save() = 0;
};

}  // namespace frz

#endif  // FRZ_GIT_IMPL_HH_
