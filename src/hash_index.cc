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

#include "hash_index.hh"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_cat.h>
#include <filesystem>
#include <memory>

#include "base32.hh"
#include "exceptions.hh"
#include "hash.hh"
#include "log.hh"

namespace frz {
namespace {

template <int HashBits>
class RamHashIndex final : public HashIndex<HashBits> {
  public:
    bool Insert(const HashAndSize<HashBits>& hs,
                const std::filesystem::path& path) override {
        auto [iter, inserted] = index_.try_emplace(hs, path);
        return inserted;
    }

    bool Contains(const HashAndSize<HashBits>& hs) const override {
        return index_.contains(hs);
    }

    void Scrub(Log& /*log*/,
               std::function<bool(const HashAndSize<HashBits>& hs,
                                  const std::filesystem::path& path)>
                   is_good) override {
        absl::erase_if(index_, [&](const auto& item) {
            const auto& [key, value] = item;
            return !is_good(key, value);
        });
    }

  private:
    absl::flat_hash_map<HashAndSize<HashBits>, std::filesystem::path> index_;
};

// Return a copy of the argument.
template <typename T>
T Copy(const T& x) {
    return x;
}

template <int HashBits>
class DiskHashIndex final : public HashIndex<HashBits> {
  public:
    DiskHashIndex(const std::filesystem::path& index_dir)
        : index_dir_(index_dir) {}

    bool Insert(const HashAndSize<HashBits>& hs,
                const std::filesystem::path& path) override try {
        std::filesystem::directory_entry symlink(index_dir_ /
                                                 SymlinkPath(hs.ToBase32()));
        if (symlink.is_symlink()) {
            return false;
        } else if (symlink.exists()) {
            throw Error("%s exists but is not a symlink", symlink.path());
        }
        const std::filesystem::path symlink_dir =
            Copy(symlink.path()).remove_filename();
        std::filesystem::create_directories(symlink_dir);
        const std::filesystem::path symlink_target =
            path.lexically_normal().lexically_proximate(
                symlink_dir.lexically_normal());
        std::filesystem::create_symlink(symlink_target, symlink.path());
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        throw Error(e.what());
    }

    bool Contains(const HashAndSize<HashBits>& hs) const override try {
        std::filesystem::directory_entry symlink(index_dir_ /
                                                 SymlinkPath(hs.ToBase32()));
        if (symlink.is_symlink()) {
            return true;
        } else if (symlink.exists()) {
            throw Error("%s exists but is not a symlink", symlink.path());
        } else {
            return false;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        throw Error(e.what());
    }

    void Scrub(Log& log, std::function<bool(const HashAndSize<HashBits>& hs,
                                            const std::filesystem::path& path)>
                             is_good) override try {
        std::filesystem::file_status stat =
            std::filesystem::symlink_status(index_dir_);
        if (std::filesystem::is_directory(stat)) {
            ScrubDir(log, is_good, index_dir_, "");
        } else if (std::filesystem::exists(stat)) {
            throw Error("%s is not a directory", index_dir_);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        throw Error(e.what());
    }

  private:
    void ScrubDir(Log& log,
                  std::function<bool(const HashAndSize<HashBits>& hs,
                                     const std::filesystem::path& path)>
                      is_good,
                  const std::filesystem::path& dir, std::string_view prefix) {
        std::vector<std::filesystem::path> to_remove;
        for (const std::filesystem::directory_entry& dent :
             std::filesystem::directory_iterator(dir)) {
            if (prefix.size() == kSymlinkSubdirs * kSymlinkSubdirDigits) {
                // We expect symlinks here, no subdirs.
                const std::optional<HashAndSize<256>> hs =
                    HashAndSize<256>::FromBase32(
                        absl::StrCat(prefix, dent.path().filename().string()));
                if (!dent.is_symlink()) {
                    log.Info("Removing %s because it isn't a symlink.",
                             dent.path());
                    to_remove.push_back(dent.path());
                } else if (!hs.has_value()) {
                    log.Info("Removing %s because its filename is not a hash.",
                             dent.path());
                    to_remove.push_back(dent.path());
                } else if (!is_good(*hs, dent.path().parent_path() /
                                             std::filesystem::read_symlink(
                                                 dent.path()))) {
                    // We don't log here, because we expect `is_good` to do so.
                    to_remove.push_back(dent.path());
                }
            } else {
                // We expect subdirs here, no symlinks.
                const std::string dirname = dent.path().filename();
                if (!std::filesystem::is_directory(dent.symlink_status())) {
                    log.Info("Removing %s because it's not a directory.",
                             dent.path());
                    to_remove.push_back(dent.path());
                } else if (dirname.size() != kSymlinkSubdirDigits ||
                           !IsBase32Number(dirname)) {
                    log.Info("Removing %s because its name is malformed.",
                             dent.path());
                    to_remove.push_back(dent.path());
                } else {
                    ScrubDir(log, is_good, dent.path(),
                             absl::StrCat(prefix, dirname));
                }
            }
        }
        for (const std::filesystem::path& p : to_remove) {
            std::filesystem::remove_all(p);
        }
    }

    const std::filesystem::path index_dir_;
};

}  // namespace

std::unique_ptr<HashIndex<256>> CreateRamHashIndex() {
    return std::make_unique<RamHashIndex<256>>();
}

std::unique_ptr<HashIndex<256>> CreateDiskHashIndex(
    const std::filesystem::path& index_dir) {
    return std::make_unique<DiskHashIndex<256>>(index_dir);
}

}  // namespace frz
