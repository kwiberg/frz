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

#include "openssl_sha256_hasher.hh"

#include <cstddef>
#include <memory>
#include <openssl/sha.h>
#include <span>

#include "assert.hh"

namespace frz {

namespace {

class OpensslSha256Hasher final : public Hasher<256> {
  public:
    OpensslSha256Hasher() : ctx_live_(true) { SHA256_Init(&ctx_); }

    ~OpensslSha256Hasher() override {
        if (ctx_live_) {
            unsigned char dummy[SHA256_DIGEST_LENGTH];
            SHA256_Final(dummy, &ctx_);
        }
    }

    void AddBytes(std::span<const std::byte> bytes) override {
        FRZ_ASSERT(ctx_live_);
        SHA256_Update(&ctx_, bytes.data(), bytes.size());
    }

    Hash<256> Finish() override {
        FRZ_ASSERT(ctx_live_);
        std::byte hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(reinterpret_cast<unsigned char*>(hash), &ctx_);
        ctx_live_ = false;
        return Hash<256>(hash);
    }

  private:
    bool ctx_live_;
    SHA256_CTX ctx_;
};

}  // namespace

std::unique_ptr<Hasher<256>> CreateOpensslSha256Hasher() {
    return std::make_unique<OpensslSha256Hasher>();
}

}  // namespace frz
