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

#ifndef FRZ_FILESYSTEM_UTIL_HH_
#define FRZ_FILESYSTEM_UTIL_HH_

#include <filesystem>
#include <optional>

namespace frz {

// If `path` is below `subtree_root`, return a relative path `p` without ..
// elements such that `subtree_root / p` refers to the same file as `path`. If
// `path` is not below `subtree_root`, return nullopt.
std::optional<std::filesystem::path> RelativeSubtreePath(
    const std::filesystem::path path, const std::filesystem::path subtree_root);

// Does this file lack all write prmissions?
bool IsReadonly(const std::filesystem::file_status& status);

// Remove all write permissions from `path`.
void RemoveWritePermissions(const std::filesystem::path path);

}  // namespace frz

#endif  // FRZ_FILESYSTEM_UTIL_HH_
