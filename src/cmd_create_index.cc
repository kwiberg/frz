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
#include <string>

#include "blake3_256_hasher.hh"
#include "exceptions.hh"
#include "file_source.hh"
#include "hash_index.hh"
#include "hasher.hh"
#include "stream.hh"

namespace frz {
namespace {

int main(int argc, char** argv) {
    CLI::App app("Create an index directory for a given content directory");

    std::string content_dir;
    app.add_option("-c,--content-dir", content_dir, "Content directory")
        ->required();

    std::string index_dir;
    app.add_option("-i,--index-dir", index_dir, "Index directory")->required();

    CLI11_PARSE(app, argc, argv);

    const std::unique_ptr<HashIndex<256>> index =
        CreateDiskHashIndex(index_dir);
    const std::unique_ptr<Streamer> streamer = CreateMultiThreadedStreamer(
        {.num_buffers = 4, .bytes_per_buffer = 1024 * 1024});
    std::int64_t successful = 0;
    std::int64_t duplicates = 0;
    std::int64_t nonfiles = 0;
    std::int64_t errors = 0;
    for (const std::filesystem::directory_entry& dent :
         std::filesystem::recursive_directory_iterator(content_dir)) {
        try {
            if (std::filesystem::is_directory(dent.symlink_status())) {
                continue;
            } else if (!std::filesystem::is_regular_file(
                           dent.symlink_status())) {
                ++nonfiles;
                continue;
            }
            auto source = CreateFileSource(dent.path());
            SizeHasher hasher(CreateBlake3_256Hasher());
            streamer->Stream(*source, hasher);
            auto hs = hasher.Finish();
            const bool inserted = index->Insert(hs, dent.path());
            if (inserted) {
                ++successful;
            } else {
                ++duplicates;
            }
            absl::PrintF("%s %s\n", inserted ? "+" : "=", dent.path());
        } catch (const Error& e) {
            ++errors;
            absl::PrintF("*** %s\n *- %s\n", dent.path(), e.what());
        }
    }

    absl::PrintF(
        "\n"
        "%d files successfully indexed\n"
        "%d files ignored because they were duplicates\n"
        "%d directory entries skipped because they weren't regular files\n"
        "%d files skipped because of errors\n",
        successful, duplicates, nonfiles, errors);
    return errors == 0 ? 0 : 1;
}

}  // namespace
}  // namespace frz

int main(int argc, char** argv) {
    return frz::main(argc, argv);
}
