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

#include "filesystem_testing.hh"

#include <absl/random/random.h>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>

#include "base32.hh"

namespace frz {
namespace {

constexpr std::string_view DescribeFileType(std::filesystem::file_type type) {
    switch (type) {
        case std::filesystem::file_type::regular:
            return "regular file";
        case std::filesystem::file_type::directory:
            return "directory";
        case std::filesystem::file_type::symlink:
            return "symlink";
        case std::filesystem::file_type::block:
            return "block device";
        case std::filesystem::file_type::character:
            return "character device";
        case std::filesystem::file_type::fifo:
            return "fifo";
        case std::filesystem::file_type::socket:
            return "socket";
        case std::filesystem::file_type::not_found:
            return "doesn't exist";
        default:
            return "unknown type of filesystem object";
    };
}

class FileTypeMatcher final {
  public:
    using is_gtest_matcher = void;

    FileTypeMatcher(std::filesystem::file_type expected_type)
        : expected_type_(expected_type) {}

    bool MatchAndExplain(const std::filesystem::path& path,
                         std::ostream* out) const {
        auto type = std::filesystem::symlink_status(path).type();
        if (type == expected_type_) {
            return true;
        } else {
            if (out != nullptr) {
                *out << DescribeFileType(type);
            }
            return false;
        }
    }

    void DescribeTo(std::ostream* out) const { Desc("is", out); }
    void DescribeNegationTo(std::ostream* out) const { Desc("is not", out); }

  private:
    void Desc(std::string_view prefix, std::ostream* out) const {
        *out << prefix << " a " << DescribeFileType(expected_type_);
    }

    std::filesystem::file_type expected_type_;
};

class SymlinkMatcher final {
  public:
    using is_gtest_matcher = void;

    SymlinkMatcher(testing::Matcher<std::filesystem::path> target_matcher)
        : target_matcher_(std::move(target_matcher)) {}

    bool MatchAndExplain(const std::filesystem::path& path,
                         std::ostream* out) const {
        if (auto type = std::filesystem::symlink_status(path).type();
            type != std::filesystem::file_type::symlink) {
            if (out != nullptr) {
                *out << DescribeFileType(type);
            }
            return false;
        }
        if (auto target = std::filesystem::read_symlink(path);
            !target_matcher_.Matches(target)) {
            if (out != nullptr) {
                *out << "is a symlink whose target is " << target;
            }
            return false;
        }
        return true;
    }

    void DescribeTo(std::ostream* out) const { Desc("is", out); }
    void DescribeNegationTo(std::ostream* out) const { Desc("is not", out); }

  private:
    void Desc(std::string_view prefix, std::ostream* out) const {
        *out << prefix << " a symlink whose target ";
        target_matcher_.DescribeTo(out);
    }

    testing::Matcher<std::filesystem::path> target_matcher_;
};

class FileContentMatcher final {
  public:
    using is_gtest_matcher = void;

    FileContentMatcher(testing::Matcher<std::string> content_matcher)
        : content_matcher_(std::move(content_matcher)) {}

    bool MatchAndExplain(const std::filesystem::path& path,
                         std::ostream* out) const {
        std::ostringstream content;
        std::ifstream in(path, std::ios::binary);
        content << in.rdbuf();
        if (content_matcher_.Matches(content.str())) {
            return true;
        } else {
            if (out != nullptr) {
                *out << "is a filesystem path whose content is \""
                     << content.str() << "\"";
            }
            return false;
        }
    }

    void DescribeTo(std::ostream* out) const { Desc("is", out); }
    void DescribeNegationTo(std::ostream* out) const { Desc("is not", out); }

  private:
    void Desc(std::string_view prefix, std::ostream* out) const {
        *out << prefix << " a filesystem path whose content ";
        content_matcher_.DescribeTo(out);
    }

    testing::Matcher<std::string> content_matcher_;
};

}  // namespace

testing::Matcher<std::filesystem::path> IsRegularFile() {
    return FileTypeMatcher(std::filesystem::file_type::regular);
}

testing::Matcher<std::filesystem::path> IsNotFound() {
    return FileTypeMatcher(std::filesystem::file_type::not_found);
}

testing::Matcher<std::filesystem::path> IsSymlinkWhoseTarget(
    testing::Matcher<std::filesystem::path> target_matcher) {
    return SymlinkMatcher(std::move(target_matcher));
}

testing::Matcher<std::filesystem::path> ReadContents(
    testing::Matcher<std::string> content_matcher) {
    return FileContentMatcher(std::move(content_matcher));
}

std::vector<std::filesystem::path> RecursiveListDirectory(
    const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> result;
    for (const std::filesystem::directory_entry& dent :
         std::filesystem::recursive_directory_iterator(dir)) {
        if (!std::filesystem::is_directory(dent.symlink_status())) {
            result.push_back(dent.path());
        }
    }
    return result;
}

std::filesystem::path TempDir::CreateTempDir() {
    absl::BitGen bitgen;
    std::filesystem::path d = std::filesystem::temp_directory_path() / "_";
    while (true) {
        d += kBase32Digits[absl::Uniform(bitgen, 0u, std::size(kBase32Digits))];
        try {
            if (std::filesystem::create_directory(d)) {
                // New directory successfully created.
                return d;
            } else {
                // A directory with this name already existed; try another,
                // longer, random path name.
            }
        } catch (const std::filesystem::filesystem_error& e) {
            if (e.code() == std::errc::file_exists) {
                // Something (not a directory) with this name already
                // existed; try another, longer, random path name.
            } else {
                throw;  // Unexpected error; re-throw.
            }
        }
    }
}

}  // namespace frz
