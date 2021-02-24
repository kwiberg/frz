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

#include "openssl_sha512_256_hasher.hh"

#include <cstddef>
#include <memory>
#include <openssl/evp.h>
#include <span>

#include "assert.hh"

namespace frz {

namespace {

class OpensslSha512_256Hasher final : public Hasher<256> {
  public:
    OpensslSha512_256Hasher() : ctx_(EVP_MD_CTX_new()) {
        FRZ_ASSERT_NE(ctx_, nullptr);
        EVP_DigestInit_ex(ctx_, EVP_sha512_256(), nullptr);
    }

    ~OpensslSha512_256Hasher() override {
        if (ctx_ != nullptr) {
            EVP_MD_CTX_free(ctx_);
        }
    }

    void AddBytes(std::span<const std::byte> bytes) override {
        FRZ_ASSERT_NE(ctx_, nullptr);
        EVP_DigestUpdate(ctx_, bytes.data(), bytes.size());
    }

    Hash<256> Finish() override {
        FRZ_ASSERT_NE(ctx_, nullptr);
        std::byte hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;
        EVP_DigestFinal_ex(ctx_, reinterpret_cast<unsigned char*>(hash),
                           &hash_len);
        EVP_MD_CTX_free(ctx_);
        ctx_ = nullptr;
        FRZ_ASSERT_EQ(hash_len, Hash<256>::kNumBytes);
        return Hash<256>(std::span<std::byte, Hash<256>::kNumBytes>(
            hash, Hash<256>::kNumBytes));
    }

  private:
    EVP_MD_CTX* ctx_;
};

}  // namespace

std::unique_ptr<Hasher<256>> CreateOpensslSha512_256Hasher() {
    return std::make_unique<OpensslSha512_256Hasher>();
}

}  // namespace frz
