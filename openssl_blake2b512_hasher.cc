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

#include "openssl_blake2b512_hasher.hh"

#include <cstddef>
#include <memory>
#include <openssl/evp.h>
#include <span>

#include "assert.hh"

namespace frz {

namespace {

class OpensslBlake2b512Hasher final : public Hasher<512> {
  public:
    OpensslBlake2b512Hasher() : ctx_(EVP_MD_CTX_new()) {
        FRZ_ASSERT_NE(ctx_, nullptr);
        EVP_DigestInit_ex(ctx_, EVP_blake2b512(), nullptr);
    }

    ~OpensslBlake2b512Hasher() override {
        if (ctx_ != nullptr) {
            EVP_MD_CTX_free(ctx_);
        }
    }

    void AddBytes(std::span<const std::byte> bytes) override {
        FRZ_ASSERT_NE(ctx_, nullptr);
        EVP_DigestUpdate(ctx_, bytes.data(), bytes.size());
    }

    Hash<512> Finish() override {
        FRZ_ASSERT_NE(ctx_, nullptr);
        std::byte hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;
        EVP_DigestFinal_ex(ctx_, reinterpret_cast<unsigned char*>(hash),
                           &hash_len);
        EVP_MD_CTX_free(ctx_);
        ctx_ = nullptr;
        FRZ_ASSERT_EQ(hash_len, Hash<512>::kNumBytes);
        return Hash<512>(std::span<std::byte, Hash<512>::kNumBytes>(
            hash, Hash<512>::kNumBytes));
    }

  private:
    EVP_MD_CTX* ctx_;
};

}  // namespace

std::unique_ptr<Hasher<512>> CreateOpensslBlake2b512Hasher() {
    return std::make_unique<OpensslBlake2b512Hasher>();
}

}  // namespace frz
