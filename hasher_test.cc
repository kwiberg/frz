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

#include "hasher.hh"

#include <cstddef>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <span>
#include <string_view>
#include <vector>

#include "blake3_256_hasher.hh"
#include "nettle_md5_hasher.hh"
#include "nettle_sha256_hasher.hh"
#include "nettle_sha3_256_hasher.hh"
#include "nettle_sha3_512_hasher.hh"
#include "nettle_sha512_256_hasher.hh"
#include "nettle_sha512_hasher.hh"
#include "openssl_blake2b512_hasher.hh"
#include "openssl_md5_hasher.hh"
#include "openssl_sha256_hasher.hh"
#include "openssl_sha512_256_hasher.hh"
#include "openssl_sha512_hasher.hh"

namespace frz {
namespace {

std::span<const std::byte> Bytes(std::string_view s) {
    return std::span(reinterpret_cast<const std::byte*>(s.data()), s.size());
}

TEST(TestBlake3x256Hasher, TestVector0) {
    // Test vector from
    // https://github.com/BLAKE3-team/BLAKE3/blob/0.3.7/test_vectors/test_vectors.json
    auto expected = Hash<256>::FromHex(
        "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262");
    auto h1 = CreateBlake3_256Hasher();
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateBlake3_256Hasher();
    h2->AddBytes(Bytes(""));
    EXPECT_EQ(h2->Finish(), expected);
}

std::vector<std::byte> CreateInputData(int size) {
    std::vector<std::byte> v;
    for (int i = 0; i < size; ++i) {
        v.push_back(static_cast<std::byte>(i % 251));
    }
    return v;
}

TEST(TestBlake3x256Hasher, TestVector3) {
    // Test vector from
    // https://github.com/BLAKE3-team/BLAKE3/blob/0.3.7/test_vectors/test_vectors.json
    const auto input = CreateInputData(3);
    auto expected = Hash<256>::FromHex(
        "e1be4d7a8ab5560aa4199eea339849ba8e293d55ca0a81006726d184519e647f");
    auto h1 = CreateBlake3_256Hasher();
    h1->AddBytes(input);
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateBlake3_256Hasher();
    h2->AddBytes(std::span(input).first(1));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(std::span(input).last(2));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestBlake3x256Hasher, TestVector6144) {
    // Test vector from
    // https://github.com/BLAKE3-team/BLAKE3/blob/0.3.7/test_vectors/test_vectors.json
    const auto input = CreateInputData(6144);
    auto expected = Hash<256>::FromHex(
        "3e2e5b74e048f3add6d21faab3f83aa44d3b2278afb83b80b3c35164ebeca205");
    auto h1 = CreateBlake3_256Hasher();
    h1->AddBytes(input);
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateBlake3_256Hasher();
    h2->AddBytes(std::span(input).first(10));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(std::span(input).last(6134));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestOpensslBlake2b512Hasher, TestVector) {
    // Test vector from https://tools.ietf.org/html/rfc7693.
    auto expected = Hash<512>::FromHex(
        "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1"
        "7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923");
    auto h1 = CreateOpensslBlake2b512Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateOpensslBlake2b512Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestNettleMd5Hasher, TestVector) {
    // Test vector from https://tools.ietf.org/html/rfc1321.
    auto expected = Hash<128>::FromHex("900150983cd24fb0d6963f7d28e17f72");
    auto h1 = CreateNettleMd5Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateNettleMd5Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestOpensslMd5Hasher, TestVector) {
    // Test vector from https://tools.ietf.org/html/rfc1321.
    auto expected = Hash<128>::FromHex("900150983cd24fb0d6963f7d28e17f72");
    auto h1 = CreateOpensslMd5Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateOpensslMd5Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestNettleSha256Hasher, TestVector1) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<256>::FromHex(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    auto h1 = CreateNettleSha256Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateNettleSha256Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestNettleSha256Hasher, TestVector2) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<256>::FromHex(
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    auto h1 = CreateNettleSha256Hasher();
    h1->AddBytes(
        Bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateNettleSha256Hasher();
    h2->AddBytes(Bytes("abcd"));
    h2->AddBytes(Bytes("bcdecdefdefgefghfghighijhijkijkljkl"));
    h2->AddBytes(Bytes("mklmnlmno"));
    h2->AddBytes(Bytes("mnopnop"));
    h2->AddBytes(Bytes("q"));
    h2->AddBytes(Bytes(""));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestOpensslSha256Hasher, TestVector1) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<256>::FromHex(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    auto h1 = CreateOpensslSha256Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateOpensslSha256Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestOpensslSha256Hasher, TestVector2) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<256>::FromHex(
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    auto h1 = CreateOpensslSha256Hasher();
    h1->AddBytes(
        Bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateOpensslSha256Hasher();
    h2->AddBytes(Bytes("abcd"));
    h2->AddBytes(Bytes("bcdecdefdefgefghfghighijhijkijkljkl"));
    h2->AddBytes(Bytes("mklmnlmno"));
    h2->AddBytes(Bytes("mnopnop"));
    h2->AddBytes(Bytes("q"));
    h2->AddBytes(Bytes(""));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestNettleSha512Hasher, TestVector1) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<512>::FromHex(
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    auto h1 = CreateNettleSha512Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateNettleSha512Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestNettleSha512Hasher, TestVector2) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<512>::FromHex(
        "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
        "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
    auto h1 = CreateNettleSha512Hasher();
    h1->AddBytes(
        Bytes("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
              "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateNettleSha512Hasher();
    h2->AddBytes(Bytes("abcdef"));
    h2->AddBytes(Bytes("ghbcdefghicdefghijdefghijkefghijklfghijkl"));
    h2->AddBytes(Bytes("m"));
    h2->AddBytes(Bytes("ghijklmn"));
    h2->AddBytes(Bytes("hijklmnoijklmnopj"));
    h2->AddBytes(Bytes("klmnopqklmnopqrlmnopqrsmnopqrstnopq"));
    h2->AddBytes(Bytes("rstu"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestOpensslSha512Hasher, TestVector1) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<512>::FromHex(
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    auto h1 = CreateOpensslSha512Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateOpensslSha512Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestOpensslSha512Hasher, TestVector2) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<512>::FromHex(
        "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
        "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
    auto h1 = CreateOpensslSha512Hasher();
    h1->AddBytes(
        Bytes("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
              "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateOpensslSha512Hasher();
    h2->AddBytes(Bytes("abcdef"));
    h2->AddBytes(Bytes("ghbcdefghicdefghijdefghijkefghijklfghijkl"));
    h2->AddBytes(Bytes("m"));
    h2->AddBytes(Bytes("ghijklmn"));
    h2->AddBytes(Bytes("hijklmnoijklmnopj"));
    h2->AddBytes(Bytes("klmnopqklmnopqrlmnopqrsmnopqrstnopq"));
    h2->AddBytes(Bytes("rstu"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestNettleSha512x256Hasher, TestVector1) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<256>::FromHex(
        "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23");
    auto h1 = CreateNettleSha512_256Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateNettleSha512_256Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestNettleSha512x256Hasher, TestVector2) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<256>::FromHex(
        "3928e184fb8690f840da3988121d31be65cb9d3ef83ee6146feac861e19b563a");
    auto h1 = CreateNettleSha512_256Hasher();
    h1->AddBytes(
        Bytes("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
              "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateNettleSha512_256Hasher();
    h2->AddBytes(Bytes("abcdef"));
    h2->AddBytes(Bytes("ghbcdefghicdefghijdefghijkefghijklfghijkl"));
    h2->AddBytes(Bytes("m"));
    h2->AddBytes(Bytes("ghijklmn"));
    h2->AddBytes(Bytes("hijklmnoijklmnopj"));
    h2->AddBytes(Bytes("klmnopqklmnopqrlmnopqrsmnopqrstnopq"));
    h2->AddBytes(Bytes("rstu"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestOpensslSha512x256Hasher, TestVector1) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<256>::FromHex(
        "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23");
    auto h1 = CreateOpensslSha512_256Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateOpensslSha512_256Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestOpensslSha512x256Hasher, TestVector2) {
    // Test vector from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha_all.pdf.
    auto expected = Hash<256>::FromHex(
        "3928e184fb8690f840da3988121d31be65cb9d3ef83ee6146feac861e19b563a");
    auto h1 = CreateOpensslSha512_256Hasher();
    h1->AddBytes(
        Bytes("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
              "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateOpensslSha512_256Hasher();
    h2->AddBytes(Bytes("abcdef"));
    h2->AddBytes(Bytes("ghbcdefghicdefghijdefghijkefghijklfghijkl"));
    h2->AddBytes(Bytes("m"));
    h2->AddBytes(Bytes("ghijklmn"));
    h2->AddBytes(Bytes("hijklmnoijklmnopj"));
    h2->AddBytes(Bytes("klmnopqklmnopqrlmnopqrsmnopqrstnopq"));
    h2->AddBytes(Bytes("rstu"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestNettleSha3x256Hasher, TestVector) {
    // Test vector from https://www.di-mgt.com.au/sha_testvectors.html.
    auto expected = Hash<256>::FromHex(
        "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
    auto h1 = CreateNettleSha3_256Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateNettleSha3_256Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

TEST(TestNettleSha3x512Hasher, TestVector) {
    // Test vector from https://www.di-mgt.com.au/sha_testvectors.html.
    auto expected = Hash<512>::FromHex(
        "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e"
        "10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0");
    auto h1 = CreateNettleSha3_512Hasher();
    h1->AddBytes(Bytes("abc"));
    EXPECT_EQ(h1->Finish(), expected);
    auto h2 = CreateNettleSha3_512Hasher();
    h2->AddBytes(Bytes("a"));
    h2->AddBytes(Bytes(""));
    h2->AddBytes(Bytes("bc"));
    EXPECT_EQ(h2->Finish(), expected);
}

}  // namespace
}  // namespace frz
