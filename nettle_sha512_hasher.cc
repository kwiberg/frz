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

#include "nettle_sha512_hasher.hh"

#include <cstddef>
#include <memory>
#include <nettle/sha2.h>
#include <span>

namespace frz {

namespace {

class NettleSha512Hasher final : public Hasher<512> {
  public:
    NettleSha512Hasher() { sha512_init(&ctx_); }

    void AddBytes(std::span<const std::byte> bytes) override {
        sha512_update(&ctx_, bytes.size(),
                      reinterpret_cast<const uint8_t*>(bytes.data()));
    }

    Hash<512> Finish() override {
        std::byte hash[SHA512_DIGEST_SIZE];
        sha512_digest(&ctx_, std::size(hash), reinterpret_cast<uint8_t*>(hash));
        return Hash<512>(hash);
    }

  private:
    sha512_ctx ctx_;
};

}  // namespace

std::unique_ptr<Hasher<512>> CreateNettleSha512Hasher() {
    return std::make_unique<NettleSha512Hasher>();
}

}  // namespace frz
