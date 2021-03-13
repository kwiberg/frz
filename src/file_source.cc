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
    explicit FileStreamSource(const std::filesystem::path& path)
        : file_(std::fopen(path.c_str(), "rb")) {
        if (file_ == nullptr) {
            throw ErrnoError();
        }
    }

    ~FileStreamSource() override {
        if (file_ != nullptr) {
            std::fclose(file_);
        }
    }

    std::variant<BytesCopied, End> GetBytes(
        std::span<std::byte> buffer) override {
        if (std::feof(file_)) {
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
                break;
            }
        }
        return BytesCopied{.num_bytes = bytes_read};
    }

    std::int64_t GetPosition() const override {
        return FRZ_ASSERT_CAST(std::int64_t, std::ftell(file_));
    }

    void SetPosition(std::int64_t pos) override {
        if (std::fseek(file_, long{pos}, SEEK_SET) != 0) {
            throw ErrnoError();
        }
    }

  private:
    std::FILE* const file_;
};

}  // namespace

std::unique_ptr<StreamSource> CreateFileSource(
    const std::filesystem::path& path) {
    return std::make_unique<FileStreamSource>(path);
}

}  // namespace frz
