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

#ifndef FRZ_FILE_STREAM_HH_
#define FRZ_FILE_STREAM_HH_

#include <filesystem>
#include <memory>

#include "stream.hh"

namespace frz {

// Create a StreamSource that reads bytes from the given file.
std::unique_ptr<StreamSource> CreateFileSource(
    const std::filesystem::path& path);

// Create a StreamSink that writes bytes to the given file. Throw
// `FileExistsException` if the file already exists.
std::unique_ptr<StreamSink> CreateFileSink(const std::filesystem::path& path);

}  // namespace frz

#endif  // FRZ_FILE_STREAM_HH_
