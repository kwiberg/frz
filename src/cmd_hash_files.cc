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

#include <CLI/CLI.hpp>
#include <absl/strings/str_format.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <map>
#include <string>
#include <vector>

#include "blake3_256_hasher.hh"
#include "exceptions.hh"
#include "file_source.hh"
#include "hash_index.hh"
#include "hasher.hh"
#include "openssl_sha256_hasher.hh"
#include "openssl_sha512_256_hasher.hh"
#include "stream.hh"

namespace frz {
namespace {

int main(int argc, char** argv) {
    CLI::App app("Hash files and print the base32 hashes to stdout");

    std::vector<std::string> files;
    app.add_option("file", files, "Input file")->required();

    const std::map<std::string, std::unique_ptr<Hasher<256>> (*)()>
        algorithm_map = {
            {"blake3", CreateBlake3_256Hasher},
            {"sha256", CreateOpensslSha256Hasher},
            {"sha512_256", CreateOpensslSha512_256Hasher},
        };
    std::string algorithm = "blake3";
    app.add_option("-a,--algorithm", algorithm, "Hash algorithm")
        ->check(CLI::IsMember(algorithm_map));

    bool multithreading = true;
    app.add_option("-m,--multithreading", multithreading,
                   "Use multiple threads?");

    std::string index_dir;
    app.add_option("-i,--index-dir", index_dir, "Index directory");

    CLI11_PARSE(app, argc, argv);

    std::unique_ptr<HashIndex<256>> index =
        index_dir.empty() ? CreateRamHashIndex()
                          : CreateDiskHashIndex(index_dir);
    const auto& [algo_name, algo_create] = *algorithm_map.find(algorithm);
    absl::PrintF("Hashing with %s, multithreading %s\n", algo_name,
                 multithreading ? "on" : "off");
    std::int64_t total_bytes = 0;
    const std::unique_ptr<Streamer> streamer =
        multithreading
            ? CreateMultiThreadedStreamer(
                  {.num_buffers = 4, .bytes_per_buffer = 1024 * 1024})
            : CreateSingleThreadedStreamer({.buffer_size = 1024 * 1024});
    absl::Time start = absl::Now();
    for (const auto& f : files) {
        try {
            auto source = CreateFileSource(f);
            SizeHasher hasher(algo_create());
            streamer->Stream(*source, hasher);
            auto hs = hasher.Finish();
            const bool inserted = index->Insert(hs, f);
            absl::PrintF("%s %s  %s\n", inserted ? "+" : "=", hs.ToBase32(), f);
            total_bytes += hs.GetSize();
        } catch (const Error& e) {
            absl::PrintF("*** %s\n", e.what());
        }
    }
    absl::Time stop = absl::Now();
    absl::PrintF("Hashed %d bytes in %s (%.1f MiB/s)\n", total_bytes,
                 absl::FormatDuration(stop - start),
                 static_cast<double>(total_bytes) /
                     absl::ToDoubleSeconds(stop - start) / (1 << 20));

    return 0;
}

}  // namespace
}  // namespace frz

int main(int argc, char** argv) {
    return frz::main(argc, argv);
}
