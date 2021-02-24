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

#include <array>
#include <benchmark/benchmark.h>
#include <cstddef>

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

template <std::size_t Size>
constexpr std::array<std::byte, Size> CreateInputData() {
    std::array<std::byte, Size> arr;
    for (std::size_t i = 0; i < arr.size(); ++i) {
        arr[i] = static_cast<std::byte>(i % 251);
    }
    return arr;
}

void Hasher_1MB(benchmark::State& state, auto create_hasher) {
    constexpr auto input_1kB = CreateInputData<1024>();
    for (auto _ : state) {
        auto h = create_hasher();
        for (int i = 0; i < 1024; ++i) {
            h->AddBytes(input_1kB);
        }
        h->Finish();
    }
}
BENCHMARK_CAPTURE(Hasher_1MB, Blake3_256, CreateBlake3_256Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, NettleMd5, CreateNettleMd5Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, NettleSha256, CreateNettleSha256Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, NettleSha3_256, CreateNettleSha256Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, NettleSha3_512, CreateNettleSha256Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, NettleSha512, CreateNettleSha512Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, NettleSha512_256, CreateNettleSha512_256Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, OpensslBlake2b512, CreateOpensslBlake2b512Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, OpensslMd5, CreateOpensslMd5Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, OpensslSha256, CreateOpensslSha256Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, OpensslSha512, CreateOpensslSha512Hasher);
BENCHMARK_CAPTURE(Hasher_1MB, OpensslSha512_256, CreateOpensslSha512_256Hasher);

}  // namespace
}  // namespace frz

BENCHMARK_MAIN();
