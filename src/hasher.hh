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

#ifndef FRZ_HASHER_HH_
#define FRZ_HASHER_HH_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "hash.hh"
#include "stream.hh"

namespace frz {

// A StreamSink that, once it has finished accepting bytes, can produce a hash
// value.
template <std::size_t NumBits>
class Hasher : public StreamSink {
  public:
    // After the last call to AddBytes, compute the hash of all the added
    // bytes. May only be called once.
    virtual Hash<NumBits> Finish() = 0;
};

// Utility StreamSink class that wraps a Hasher, and additionally counts the
// number of bytes streaming through.
template <std::size_t NumBits>
class SizeHasher final : public StreamSink {
  public:
    SizeHasher(std::unique_ptr<Hasher<NumBits>> hasher)
        : hasher_(std::move(hasher)) {
        FRZ_ASSERT_NE(hasher_, nullptr);
    }

    void AddBytes(std::span<const std::byte> buffer) override {
        FRZ_ASSERT_NE(hasher_, nullptr);
        hasher_->AddBytes(buffer);
        num_bytes_ += buffer.size();
    }

    HashAndSize<NumBits> Finish() {
        FRZ_ASSERT_NE(hasher_, nullptr);
        Hash<NumBits> hash = hasher_->Finish();
        hasher_ = nullptr;
        return HashAndSize(hash, num_bytes_);
    }

  private:
    std::int64_t num_bytes_ = 0;
    std::unique_ptr<Hasher<NumBits>> hasher_;
};

}  // namespace frz

#endif  // FRZ_HASHER_HH_
