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

#ifndef FRZ_GIT_HH_
#define FRZ_GIT_HH_

#include <filesystem>
#include <memory>

namespace frz {

// Instances of this class represent zero or more Git repositories, which are
// automatically discovered by looking in the argument directory, the parent
// directory, the grandparent directory, etc.
class Git {
  public:
    static std::unique_ptr<Git> Create();

    virtual ~Git() = default;

    // Is the given directory entry ignored by git? (If no git directory owns
    // the it, the answer is always no.)
    virtual bool IsIgnored(const std::filesystem::directory_entry& dent) = 0;
    bool IsIgnored(const std::filesystem::path path) {
        return IsIgnored(std::filesystem::directory_entry(path));
    }

    // Add the given file to the index of the git repository that owns it. (If
    // no git directory owns the it, silently do nothing.)
    virtual void Add(const std::filesystem::directory_entry& dent) = 0;
    void Add(const std::filesystem::path file) {
        Add(std::filesystem::directory_entry(file));
    }

    // Save in-memory changes to disk.
    virtual void Save() = 0;
};

}  // namespace frz

#endif  // FRZ_GIT_HH_
