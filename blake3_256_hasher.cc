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

#include "blake3_256_hasher.hh"

#include <blake3.h>
#include <cstddef>
#include <memory>
#include <span>

namespace frz {

namespace {

class Blake3_256Hasher final : public Hasher<256> {
  public:
    Blake3_256Hasher() { blake3_hasher_init(&ctx_); }

    void AddBytes(std::span<const std::byte> bytes) override {
        blake3_hasher_update(&ctx_, bytes.data(), bytes.size());
    }

    Hash<256> Finish() override {
        std::byte hash[BLAKE3_OUT_LEN];
        blake3_hasher_finalize(&ctx_, reinterpret_cast<std::uint8_t*>(hash),
                               std::size(hash));
        return Hash<256>(hash);
    }

  private:
    blake3_hasher ctx_;
};

}  // namespace

std::unique_ptr<Hasher<256>> CreateBlake3_256Hasher() {
    return std::make_unique<Blake3_256Hasher>();
}

}  // namespace frz
