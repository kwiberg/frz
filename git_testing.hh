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

#ifndef FRZ_GIT_TESTING_HH_
#define FRZ_GIT_TESTING_HH_

#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

namespace frz {

testing::Matcher<std::filesystem::path> GitStatus(
    testing::Matcher<std::vector<std::string_view>>);

// Create a new git repository in the specified directory.
void CreateGitRepository(const std::filesystem::path& dir);

}  // namespace frz

#endif  // FRZ_GIT_TESTING_HH_
