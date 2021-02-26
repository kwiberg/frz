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

#include "hash.hh"

#include <absl/random/random.h>
#include <absl/strings/str_format.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <string>
#include <string_view>

#include "assert.hh"

namespace frz {
namespace {

using ::testing::Optional;
using ::testing::StrCaseEq;

// Convert a char array to a string.
std::string Str(std::span<const char> s) {
    return std::string(std::cbegin(s), std::cend(s));
}

TEST(TestHash, ToHex) {
    EXPECT_EQ(Str(Hash<8>(std::array{std::byte{0x4a}}).ToHex()), "4a");
    EXPECT_EQ(Str(Hash<16>(std::array{std::byte{0}, std::byte{1}}).ToHex()),
              "0001");
}

TEST(TestHash, FromHex) {
    EXPECT_EQ(Hash<8>::FromHex(""), std::nullopt);
    EXPECT_EQ(Hash<8>::FromHex("4"), std::nullopt);
    EXPECT_EQ(Hash<8>::FromHex("477"), std::nullopt);
    EXPECT_EQ(Hash<8>::FromHex("47x"), std::nullopt);
    EXPECT_EQ(Hash<8>::FromHex("4x"), std::nullopt);
    EXPECT_EQ(Hash<8>::FromHex("47"), Hash<8>(std::array{std::byte{0x47}}));
    EXPECT_EQ(Hash<24>::FromHex("123a5B"),
              Hash<24>(std::array{std::byte{0x12}, std::byte{0x3a},
                                  std::byte{0x5B}}));
}

template <std::size_t HashBits>
constexpr HashAndSize<HashBits> HaS(std::string_view hex,
                                    std::int64_t num_bytes) {
    std::optional<Hash<HashBits>> h = Hash<HashBits>::FromHex(hex);
    FRZ_CHECK(h.has_value());
    return HashAndSize(*h, num_bytes);
}

TEST(TestHashAndSize, Comparison) {
    // Test the actual operators.
    EXPECT_TRUE(HaS<8>("aa", 1) == HaS<8>("aa", 1));
    EXPECT_TRUE(HaS<8>("aa", 1) != HaS<8>("aa", 2));
    EXPECT_TRUE(HaS<8>("aa", 1) != HaS<8>("bb", 1));
    EXPECT_TRUE(HaS<8>("aa", 1) != HaS<8>("bb", 2));

    // Test more combinations.
    EXPECT_EQ(HaS<8>("bb", 1), HaS<8>("bb", 1));
    EXPECT_NE(HaS<8>("aa", 2), HaS<8>("aa", 1));
    EXPECT_NE(HaS<8>("bb", 1), HaS<8>("aa", 1));
    EXPECT_NE(HaS<8>("bb", 2), HaS<8>("aa", 1));
}

TEST(TestHashAndSize, ToBase32) {
    EXPECT_EQ(HaS<8>("aa", 0).ToBase32(), "n8");
    EXPECT_EQ(HaS<8>("aa", 1).ToBase32(), "n9");
    EXPECT_EQ(HaS<8>("aa", 2).ToBase32(), "na");
    EXPECT_EQ(HaS<8>("aa", 4).ToBase32(), "n84");
    EXPECT_EQ(HaS<8>("aa", 128).ToBase32(), "n840");
    EXPECT_EQ(HaS<24>("000000", 0).ToBase32(), "00000");
    EXPECT_EQ(HaS<24>("000000", 1).ToBase32(), "00001");
    EXPECT_EQ(HaS<24>("000000", 2).ToBase32(), "000002");
    EXPECT_EQ(
        HaS<128>("000102030405060708090a0b0c0d0e0f", 1234567890).ToBase32(),
        "000g40r40m30e209185gr38e1x4sc0pj");
}

template <std::size_t HashBits>
constexpr std::optional<HashAndSize<HashBits>> HaS(std::string_view base32) {
    return HashAndSize<HashBits>::FromBase32(base32);
}

TEST(TestHashAndSize, FromBase32) {
    EXPECT_THAT(HashAndSize<8>::FromBase32("n8"), Optional(HaS<8>("aa", 0)));
    EXPECT_THAT(HashAndSize<8>::FromBase32("n9"), Optional(HaS<8>("aa", 1)));
    EXPECT_THAT(HashAndSize<8>::FromBase32("na"), Optional(HaS<8>("aa", 2)));
    EXPECT_THAT(HashAndSize<8>::FromBase32("n84"), Optional(HaS<8>("aa", 4)));
    EXPECT_THAT(HashAndSize<8>::FromBase32("n840"),
                Optional(HaS<8>("aa", 128)));
    EXPECT_THAT(HashAndSize<24>::FromBase32("00000"),
                Optional(HaS<24>("000000", 0)));
    EXPECT_THAT(HashAndSize<24>::FromBase32("00001"),
                Optional(HaS<24>("000000", 1)));
    EXPECT_THAT(HashAndSize<24>::FromBase32("000002"),
                Optional(HaS<24>("000000", 2)));
    EXPECT_THAT(
        HashAndSize<128>::FromBase32("000g40r40m30e209185gr38e1x4sc0pj"),
        Optional(HaS<128>("000102030405060708090a0b0c0d0e0f", 1234567890)));
}

template <std::size_t HashBits>
HashAndSize<HashBits> RandomHaS(absl::BitGen& bitgen) {
    std::array<std::byte, Hash<HashBits>::kNumBytes> hash_bytes;
    for (int i = 0; i < std::ssize(hash_bytes); ++i) {
        hash_bytes[i] = std::byte(absl::Uniform(bitgen, 0, 255));
    }
    return HashAndSize(
        Hash<HashBits>(hash_bytes),
        absl::LogUniform(bitgen, std::int64_t{0},
                         std::numeric_limits<std::int64_t>::max()));
}

constexpr int kNumRandomIterations = 10000;

TEST(TestHashAndSize, Base32Roundtrip) {
    // TODO(github.com/kwiberg/frz/issues/6): Replace this test with a real
    // fuzzer.
    absl::BitGen bitgen;
    for (int i = 0; i < kNumRandomIterations; ++i) {
        const HashAndSize<256> hs = RandomHaS<256>(bitgen);
        ASSERT_THAT(HashAndSize<256>::FromBase32(hs.ToBase32()), Optional(hs));
    }
}

std::string RandomString(absl::BitGen& bitgen, std::string_view alphabet = "") {
    std::string s;
    while (absl::Bernoulli(bitgen, 0.99)) {
        if (alphabet.empty()) {
            s += absl::Uniform(bitgen, std::numeric_limits<char>::min(),
                               std::numeric_limits<char>::max());
        } else {
            s += alphabet[absl::Uniform(bitgen, 0u, alphabet.size() - 1)];
        }
    }
    return s;
}

TEST(TestHashAndSize, Base32Junk) {
    // TODO(github.com/kwiberg/frz/issues/6): Replace this test with a real
    // fuzzer.
    absl::BitGen bitgen;
    for (int i = 0; i < kNumRandomIterations; ++i) {
        const std::string s = RandomString(bitgen);
        SCOPED_TRACE(s);
        HashAndSize<256>::FromBase32(s);
    }
}

TEST(TestHashAndSize, Base32JunkOrReverseRoundtrip) {
    // TODO(github.com/kwiberg/frz/issues/6): Replace this test with a real
    // fuzzer.
    constexpr std::string_view all_base32_digits =
        "0123456789abcdefghjkmnpqrstuwxyzABCDEFGHJKMNPQRSTUWXYZ";
    absl::BitGen bitgen;
    int good = 0;
    for (int i = 0; i < kNumRandomIterations; ++i) {
        const std::string s = RandomString(bitgen, all_base32_digits);
        SCOPED_TRACE(s);
        const std::optional<HashAndSize<256>> hs =
            HashAndSize<256>::FromBase32(s);
        if (hs.has_value()) {
            ASSERT_THAT(hs->ToBase32(), StrCaseEq(s));
            ++good;
        }
    }
    absl::PrintF("%d of %d random strings were legal.\n", good,
                 kNumRandomIterations);
}

}  // namespace
}  // namespace frz
