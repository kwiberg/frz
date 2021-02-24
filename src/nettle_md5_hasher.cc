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

#include "nettle_md5_hasher.hh"

#include <cstddef>
#include <memory>
#include <nettle/md5.h>
#include <span>

namespace frz {

namespace {

class NettleMd5Hasher final : public Hasher<128> {
  public:
    NettleMd5Hasher() { md5_init(&ctx_); }

    void AddBytes(std::span<const std::byte> bytes) override {
        md5_update(&ctx_, bytes.size(),
                   reinterpret_cast<const uint8_t*>(bytes.data()));
    }

    Hash<128> Finish() override {
        std::byte hash[MD5_DIGEST_SIZE];
        md5_digest(&ctx_, std::size(hash), reinterpret_cast<uint8_t*>(hash));
        return Hash<128>(hash);
    }

  private:
    md5_ctx ctx_;
};

}  // namespace

std::unique_ptr<Hasher<128>> CreateNettleMd5Hasher() {
    return std::make_unique<NettleMd5Hasher>();
}

}  // namespace frz
