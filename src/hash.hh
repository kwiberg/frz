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

#ifndef FRZ_HASH_HH_
#define FRZ_HASH_HH_

#include <absl/numeric/int128.h>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>

#include "assert.hh"
#include "base32.hh"
#include "math.hh"

namespace frz {

namespace frz_hash_impl {

// The traditional base-16 digit set.
inline constexpr std::string_view kHexDigits = "0123456789abcdef";
static_assert(kHexDigits.size() == 16);

inline constexpr std::optional<int> HexToVal(char c) {
    if ('0' <= c && c <= '9') {
        return c - '0';
    }
    if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    }
    if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    }
    return std::nullopt;
}

template <std::size_t NumBits>
constexpr bool ParseHex(std::span<const char, NumBits / 4> hex,
                        std::span<std::byte, NumBits / 8> bytes) {
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        std::optional<int> a = HexToVal(hex[2 * i]);
        std::optional<int> b = HexToVal(hex[2 * i + 1]);
        if (a.has_value() && b.has_value()) {
            bytes[i] = static_cast<std::byte>((*a << 4) + *b);
        } else {
            return false;
        }
    }
    return true;
}

}  // namespace frz_hash_impl

template <std::size_t NumBits>
class Hash final {
  public:
    static_assert(NumBits > 0);
    static_assert(NumBits % 8 == 0);
    static inline constexpr std::size_t kNumBytes = NumBits / 8;

    static constexpr std::optional<Hash> FromHex(std::string_view hex) {
        if (4 * hex.size() != NumBits) {
            return std::nullopt;
        }
        std::array<std::byte, kNumBytes> hash;
        if (frz_hash_impl::ParseHex<NumBits>(
                std::span<const char, NumBits / 4>(hex), hash)) {
            return Hash(hash);
        } else {
            return std::nullopt;
        }
    }

    explicit constexpr Hash(std::span<const std::byte, kNumBytes> hash_bytes) {
        FRZ_ASSERT_EQ(hash_bytes.size(), hash_bytes_.size());
        std::copy(std::cbegin(hash_bytes), std::cend(hash_bytes),
                  std::begin(hash_bytes_));
    }

    constexpr Hash(const Hash&) = default;
    constexpr Hash& operator=(const Hash&) = default;

    constexpr bool operator==(const Hash&) const = default;

    template <typename H>
    friend H AbslHashValue(H h, const Hash& hash) {
        return H::combine(std::move(h), hash.hash_bytes_);
    }

    constexpr std::span<const std::byte, kNumBytes> Bytes() const {
        return hash_bytes_;
    }

    constexpr std::array<char, NumBits / 4> ToHex() const {
        std::array<char, NumBits / 4> hex;
        for (std::size_t i = 0; i < kNumBytes; ++i) {
            auto b = static_cast<std::uint8_t>(hash_bytes_[i]);
            hex[2 * i] = frz_hash_impl::kHexDigits[b >> 4];
            hex[2 * i + 1] = frz_hash_impl::kHexDigits[b & 0xf];
        }
        return hex;
    }

    // For debugging, e.g. googletest.
    friend std::ostream& operator<<(std::ostream& out, const Hash& hash) {
        auto hex = hash.ToHex();
        return out << "Hash<" << NumBits << ">:"
                   << std::string_view(std::cbegin(hex), std::cend(hex));
    }

  private:
    std::array<std::byte, kNumBytes> hash_bytes_;
};

template <std::size_t HashBits>
class HashAndSize final {
  public:
    static constexpr std::optional<HashAndSize> FromBase32(
        std::string_view base32) {
        std::uint64_t value = 0;
        int bits_in_value = 0;
        std::size_t input_index = 0;
        bool error = false;
        auto get_byte = [&] {
            if (error) {
                return std::byte(0);
            }
            while (bits_in_value < 8) {
                if (input_index >= base32.size()) {
                    error = true;
                    return std::byte(0);
                }
                std::optional<int> d = Base32ToVal(base32[input_index]);
                if (!d.has_value()) {
                    error = true;
                    return std::byte(0);
                }
                value = (value << 5) | *d;
                bits_in_value += 5;
                ++input_index;
            }
            bits_in_value -= 8;
            auto r = std::byte(value >> bits_in_value);
            value &= ~(~std::uint64_t{0} << bits_in_value);
            return r;
        };
        std::array<std::byte, Hash<HashBits>::kNumBytes> hash_bytes;
        for (std::byte& b : hash_bytes) {
            b = get_byte();
        }
        for (; !error && input_index < base32.size();
             ++input_index, bits_in_value += 5) {
            std::optional<int> d = Base32ToVal(base32[input_index]);
            if (d.has_value() && std::countl_zero(value) >= 6) {
                value = (value << 5) | *d;
            } else {
                error = true;
            }
        }
        int actual_bits_in_value = 64 - std::countl_zero(value);
        FRZ_ASSERT_LE(actual_bits_in_value, 63);
        FRZ_ASSERT_LE(actual_bits_in_value, bits_in_value);
        if (bits_in_value - actual_bits_in_value >= 5) {
            // Size was encoded with more digits than necessary. Since we want
            // base-32 representations to be 1:1 with the in-memory
            // representation, we disallow this.
            error = true;
        }
        return error ? std::nullopt
                     : std::optional(
                           HashAndSize(Hash<HashBits>(hash_bytes), value));
    }

    constexpr HashAndSize(Hash<HashBits> hash, std::int64_t size)
        : hash_(hash), size_(size) {}

    constexpr HashAndSize(const HashAndSize&) = default;
    constexpr HashAndSize& operator=(const HashAndSize&) = default;

    constexpr bool operator==(const HashAndSize&) const = default;

    template <typename H>
    friend H AbslHashValue(H h, const HashAndSize& hs) {
        return H::combine(std::move(h), hs.hash_, hs.size_);
    }

    const Hash<HashBits>& GetHash() const { return hash_; }
    std::int64_t GetSize() const { return size_; }

    std::string ToBase32() const {
        // How many output bits do we need? We store the hash using `HashBits`
        // bits. For the number of bytes, figure out how many bits we need by
        // counting leading zero bits, and then round up so that the total
        // number of bits becomes a multiple of 5.
        const auto size = FRZ_ASSERT_CAST(std::uint64_t, size_);
        const int size_bits =
            RoundUp(64 - std::countl_zero(size) + int{HashBits}, 5) -
            int{HashBits};
        const int output_bits = int{HashBits} + size_bits;
        FRZ_ASSUME_EQ(output_bits % 5, 0);
        std::string base32;
        base32.reserve(FRZ_ASSERT_CAST(std::size_t, output_bits / 5));

        // Convert all whole groups of 5 bytes from the hash to groups of 8
        // bas32 digits.
        for (std::size_t i = 0;
             i < RoundDown<std::size_t>(hash_.Bytes().size(), 5); i += 5) {
            std::uint64_t n =
                static_cast<std::uint64_t>(hash_.Bytes()[i + 0]) << 32 |
                static_cast<std::uint64_t>(hash_.Bytes()[i + 1]) << 24 |
                static_cast<std::uint64_t>(hash_.Bytes()[i + 2]) << 16 |
                static_cast<std::uint64_t>(hash_.Bytes()[i + 3]) << 8 |
                static_cast<std::uint64_t>(hash_.Bytes()[i + 4]) << 0;
            const char digits[] = {kBase32Digits[(n >> 35) & 0x1f],
                                   kBase32Digits[(n >> 30) & 0x1f],
                                   kBase32Digits[(n >> 25) & 0x1f],
                                   kBase32Digits[(n >> 20) & 0x1f],
                                   kBase32Digits[(n >> 15) & 0x1f],
                                   kBase32Digits[(n >> 10) & 0x1f],
                                   kBase32Digits[(n >> 5) & 0x1f],
                                   kBase32Digits[(n >> 0) & 0x1f]};
            static_assert(std::size(digits) == 8);
            base32.append(std::string_view(digits, 8));
        }

        // Shift the remaining 0-4 bytes of the hash into `n`, and count the
        // number of `bits` so shifted.
        int bits = 0;
        absl::uint128 n = 0;
        for (std::size_t i = RoundDown<std::size_t>(hash_.Bytes().size(), 5);
             i < hash_.Bytes().size(); ++i) {
            n = (n << 8) | static_cast<std::uint8_t>(hash_.Bytes()[i]);
            bits += 8;
        }
        FRZ_ASSUME_LE(bits, 32);

        // Shift the number of bytes into `n`.
        FRZ_ASSUME_GE(size_bits, 0);
        FRZ_ASSUME_LE(size_bits, 68);
        n = (n << size_bits) | size;
        bits += size_bits;
        FRZ_ASSUME_LE(bits, 100);
        FRZ_ASSUME_EQ(bits % 5, 0);

        // Convert `n` to base32 digits, append them to the rest, and return.
        char digits[20];
        for (int i = 0; i < bits / 5; ++i) {
            const int shift = bits - 5 * i - 5;
            digits[i] =
                kBase32Digits[static_cast<std::size_t>((n >> shift) & 0x1f)];
        }
        base32.append(
            std::string_view(digits, FRZ_ASSERT_CAST(std::size_t, bits / 5)));
        FRZ_ASSERT_EQ(base32.size(), output_bits / 5);
        return base32;
    }

    // For debugging, e.g. googletest.
    friend std::ostream& operator<<(std::ostream& out, const HashAndSize& hs) {
        auto hex = hs.hash_.ToHex();
        return out << "{hash/" << HashBits << ":"
                   << std::string_view(std::cbegin(hex), std::cend(hex))
                   << ",size:" << hs.size_ << ",base32:" << hs.ToBase32()
                   << "}";
    }

  private:
    Hash<HashBits> hash_;
    std::int64_t size_;
};

}  // namespace frz

#endif  // FRZ_HASH_HH_
