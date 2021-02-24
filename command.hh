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

#ifndef FRZ_COMMAND_HH_
#define FRZ_COMMAND_HH_

#include <filesystem>
#include <string>
#include <vector>

namespace frz {

int Command(const std::filesystem::path& working_dir,
            const std::vector<std::string> args);

int Command(const std::filesystem::path& working_dir, int argc,
            char const* const* argv);

}  // namespace frz

#endif  // FRZ_COMMAND_HH_
