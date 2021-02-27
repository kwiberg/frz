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

#include "file_source.hh"

#include <cstdio>

#include "assert.hh"
#include "exceptions.hh"
#include "stream.hh"

namespace frz {

namespace {

class FileStreamSource final : public StreamSource {
  public:
    FileStreamSource(std::FILE* file) : file_(file) {
        FRZ_ASSERT_NE(file_, nullptr);
        FRZ_CHECK(!std::ferror(file_));
    }

    ~FileStreamSource() override {
        if (file_ != nullptr) {
            std::fclose(file_);
        }
    }

    std::variant<BytesCopied, End> GetBytes(
        std::span<std::byte> buffer) override {
        if (file_ == nullptr) {
            return End{};
        }
        int bytes_read = 0;
        while (bytes_read < std::ssize(buffer)) {
            bytes_read += FRZ_ASSERT_CAST(
                int, std::fread(buffer.data() + bytes_read, 1,
                                buffer.size() - bytes_read, file_));
            if (std::ferror(file_)) {
                throw ErrnoError();
            }
            if (std::feof(file_)) {
                std::fclose(file_);
                file_ = nullptr;
                break;
            }
        }
        return BytesCopied{.num_bytes = bytes_read};
    }

  private:
    std::FILE* file_;
};

}  // namespace

std::unique_ptr<StreamSource> CreateFileSource(std::filesystem::path path) {
    std::FILE* const file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        throw ErrnoError();
    } else {
        return std::make_unique<FileStreamSource>(file);
    }
}

}  // namespace frz
