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

#include "openssl_sha512_hasher.hh"

#include <cstddef>
#include <memory>
#include <openssl/sha.h>
#include <span>

#include "assert.hh"

namespace frz {

namespace {

class OpensslSha512Hasher final : public Hasher<512> {
  public:
    OpensslSha512Hasher() : ctx_live_(true) { SHA512_Init(&ctx_); }

    ~OpensslSha512Hasher() override {
        if (ctx_live_) {
            unsigned char dummy[SHA512_DIGEST_LENGTH];
            SHA512_Final(dummy, &ctx_);
        }
    }

    void AddBytes(std::span<const std::byte> bytes) override {
        FRZ_ASSERT(ctx_live_);
        SHA512_Update(&ctx_, bytes.data(), bytes.size());
    }

    Hash<512> Finish() override {
        FRZ_ASSERT(ctx_live_);
        std::byte hash[SHA512_DIGEST_LENGTH];
        SHA512_Final(reinterpret_cast<unsigned char*>(hash), &ctx_);
        ctx_live_ = false;
        return Hash<512>(hash);
    }

  private:
    bool ctx_live_;
    SHA512_CTX ctx_;
};

}  // namespace

std::unique_ptr<Hasher<512>> CreateOpensslSha512Hasher() {
    return std::make_unique<OpensslSha512Hasher>();
}

}  // namespace frz
