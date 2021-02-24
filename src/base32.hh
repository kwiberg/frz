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

#ifndef FRZ_BASE32_HH_
#define FRZ_BASE32_HH_

#include <algorithm>
#include <concepts>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string_view>

namespace frz {

// A base-32 digit set chosen so that letters easily mistaken for digits are
// omitted.
inline constexpr std::string_view kBase32Digits =
    "0123456789abcdefghjkmnpqrstuwxyz";
static_assert(kBase32Digits.size() == 32);

inline constexpr std::optional<int> Base32ToVal(char c) {
    switch (c) {
            // clang-format off
        case '0':           return 0;
        case '1':           return 1;
        case '2':           return 2;
        case '3':           return 3;
        case '4':           return 4;
        case '5':           return 5;
        case '6':           return 6;
        case '7':           return 7;
        case '8':           return 8;
        case '9':           return 9;
        case 'a': case 'A': return 10;
        case 'b': case 'B': return 11;
        case 'c': case 'C': return 12;
        case 'd': case 'D': return 13;
        case 'e': case 'E': return 14;
        case 'f': case 'F': return 15;
        case 'g': case 'G': return 16;
        case 'h': case 'H': return 17;
        case 'j': case 'J': return 18;
        case 'k': case 'K': return 19;
        case 'm': case 'M': return 20;
        case 'n': case 'N': return 21;
        case 'p': case 'P': return 22;
        case 'q': case 'Q': return 23;
        case 'r': case 'R': return 24;
        case 's': case 'S': return 25;
        case 't': case 'T': return 26;
        case 'u': case 'U': return 27;
        case 'w': case 'W': return 28;
        case 'x': case 'X': return 29;
        case 'y': case 'Y': return 30;
        case 'z': case 'Z': return 31;
        default:            return std::nullopt;
            // clang-format on
    }
}

// clang-format off
template <typename T>
requires std::ranges::input_range<T> &&
         std::same_as<std::ranges::range_value_t<T>, char>
constexpr bool IsBase32Number(T str) {
    return std::ranges::all_of(str, [](char c) {
        return Base32ToVal(c).has_value();
    });
}
// clang-format on

// The number of subdirectory levels to use for base-32 symlink names.
inline constexpr int kSymlinkSubdirs = 2;

// The number of base-32 digits to use for each directory name.
inline constexpr int kSymlinkSubdirDigits = 2;

// Construct a symlink path for the given base-32 hash string,
inline std::filesystem::path SymlinkPath(std::string_view base32) {
    static_assert(kSymlinkSubdirs == 2);
    static_assert(kSymlinkSubdirDigits == 2);
    return std::filesystem::path(base32.substr(0, 2)) / base32.substr(2, 2) /
           base32.substr(4);
}

// Parse a base-32 number out of a symlink target path. Return nullopt if
// parsing fails.
inline std::optional<std::string> PathBase32(
    std::string_view hash_name, const std::filesystem::path& link_target) {
    std::string base32;
    int seen_elements = 0;
    for (const std::filesystem::path& element : link_target) {
        if (seen_elements == 0) {
            if (element != ".frz") {
                return std::nullopt;
            }
        } else if (seen_elements == 1) {
            if (element != hash_name) {
                return std::nullopt;
            }
        } else if (seen_elements - 2 < kSymlinkSubdirs) {
            if (element.native().size() == kSymlinkSubdirDigits &&
                IsBase32Number(element.native())) {
                base32.append(element);
            } else {
                return std::nullopt;
            }
        } else if (seen_elements - 2 == kSymlinkSubdirs) {
            if (IsBase32Number(element.native())) {
                base32.append(element);
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
        ++seen_elements;
    }
    return base32;
}

}  // namespace frz

#endif  // FRZ_BASE32_HH_
