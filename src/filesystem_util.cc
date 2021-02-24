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

#include "filesystem_util.hh"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>

namespace frz {
namespace {

constexpr auto kAllWritePermissions = std::filesystem::perms::owner_write |
                                      std::filesystem::perms::group_write |
                                      std::filesystem::perms::others_write;

}  // namespace

std::optional<std::filesystem::path> RelativeSubtreePath(
    const std::filesystem::path path,
    const std::filesystem::path subtree_root) {
    std::filesystem::path relative_path =
        path.lexically_normal()
            .lexically_proximate(subtree_root.lexically_normal())
            .lexically_normal();
    if (relative_path.is_relative() &&
        std::ranges::none_of(relative_path, [](auto&& path_element) {
            return path_element == "..";
        })) {
        return relative_path;
    } else {
        return std::nullopt;
    }
}

bool IsReadonly(const std::filesystem::file_status& status) {
    return (status.permissions() & kAllWritePermissions) ==
           std::filesystem::perms::none;
}

void RemoveWritePermissions(const std::filesystem::path path) {
    std::filesystem::permissions(path, kAllWritePermissions,
                                 std::filesystem::perm_options::remove |
                                     std::filesystem::perm_options::nofollow);
}

}  // namespace frz
