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

#include "openssl_md5_hasher.hh"

#include <cstddef>
#include <memory>
#include <openssl/md5.h>
#include <span>

#include "assert.hh"

namespace frz {

namespace {

class OpensslMd5Hasher final : public Hasher<128> {
  public:
    OpensslMd5Hasher() : ctx_live_(true) { MD5_Init(&ctx_); }

    ~OpensslMd5Hasher() override {
        if (ctx_live_) {
            unsigned char dummy[MD5_DIGEST_LENGTH];
            MD5_Final(dummy, &ctx_);
        }
    }

    void AddBytes(std::span<const std::byte> bytes) override {
        FRZ_ASSERT(ctx_live_);
        MD5_Update(&ctx_, bytes.data(), bytes.size());
    }

    Hash<128> Finish() override {
        FRZ_ASSERT(ctx_live_);
        std::byte hash[MD5_DIGEST_LENGTH];
        MD5_Final(reinterpret_cast<unsigned char*>(hash), &ctx_);
        ctx_live_ = false;
        return Hash<128>(hash);
    }

  private:
    bool ctx_live_;
    MD5_CTX ctx_;
};

}  // namespace

std::unique_ptr<Hasher<128>> CreateOpensslMd5Hasher() {
    return std::make_unique<OpensslMd5Hasher>();
}

}  // namespace frz
