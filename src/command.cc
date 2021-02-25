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

#include "command.hh"

#include <CLI/CLI.hpp>
#include <absl/algorithm/container.h>
#include <memory>
#include <string>
#include <vector>

#include "blake3_256_hasher.hh"
#include "exceptions.hh"
#include "git.hh"
#include "log.hh"
#include "stream.hh"
#include "top_directory.hh"

namespace frz {
namespace {

class ContentSourceOptions final {
  public:
    explicit ContentSourceOptions(CLI::App& app)
        : app_(app),
          copy_from_opt_(
              *app.add_option("--copy-from", copy_from_,
                              "If content is found to be missing, search this\n"
                              "directory for matching files to copy")
                   ->type_name("DIR")),
          move_from_opt_(
              *app.add_option(
                      "--move-from", move_from_,
                      "If content is found to be missing, search this\n"
                      "directory for matching files to move into\n"
                      ".frz/content (or copy, if moving isn't possible)")
                   ->type_name("DIR")) {}

    std::vector<Top::ContentSource> GetResult(
        const std::filesystem::path& working_dir) const {
        // Merge `copy_from_` and `move_from_` into a single list, interleaving
        // in the order they were given on the command line.
        std::vector<std::string> copy_from = copy_from_;
        std::vector<std::string> move_from = move_from_;
        absl::c_reverse(copy_from);
        absl::c_reverse(move_from);
        std::vector<Top::ContentSource> content_sources;
        for (const auto* option : app_.parse_order()) {
            if (option == &copy_from_opt_) {
                content_sources.push_back(
                    {.path = working_dir / copy_from.back(),
                     .read_only = true});
                copy_from.pop_back();
            } else if (option == &move_from_opt_) {
                content_sources.push_back(
                    {.path = working_dir / move_from.back(),
                     .read_only = false});
                move_from.pop_back();
            }
        }
        FRZ_ASSERT_EQ(copy_from.size(), 0);
        FRZ_ASSERT_EQ(move_from.size(), 0);
        FRZ_ASSERT_EQ(content_sources.size(),
                      copy_from_.size() + move_from_.size());
        return content_sources;
    }

  private:
    std::vector<std::string> copy_from_;
    std::vector<std::string> move_from_;
    const CLI::App& app_;
    const CLI::Option& copy_from_opt_;
    const CLI::Option& move_from_opt_;
};

struct CommonArgs {
    const std::filesystem::path& working_dir;
    Log log;
    Streamer& streamer;
    const std::unique_ptr<Top> top;
};

struct AddArgs {
    std::vector<std::string> files;
};
int Add(CommonArgs& common_args, const AddArgs& add_args) {
    std::int64_t successful = 0;
    std::int64_t duplicates = 0;
    std::int64_t nonfiles = 0;
    std::int64_t errors = 0;
    const std::unique_ptr<Git> git = Git::Create();
    auto ignored = [&](const std::filesystem::path& path) {
        return path.filename() == ".frz" || git->IsIgnored(path);
    };
    auto pretty_path = [&](const std::filesystem::path& path) {
        return path.lexically_normal().lexically_proximate(
            common_args.working_dir.lexically_normal());
    };
    auto add_file = [&](const std::filesystem::directory_entry& dent) {
        if (std::filesystem::is_directory(dent.symlink_status())) {
            return;
        } else if (!std::filesystem::is_regular_file(dent.symlink_status()) &&
                   !dent.is_symlink()) {
            ++nonfiles;
            return;
        }
        const Top::AddResult r = common_args.top->AddFile(dent.path());
        if (r == Top::AddResult::kNewFile) {
            ++successful;
            absl::PrintF("+ %s\n", pretty_path(dent.path()));
        } else if (r == Top::AddResult::kDuplicateFile) {
            ++duplicates;
            absl::PrintF("= %s\n", pretty_path(dent.path()));
        }
        git->Add(dent.path());  // don't use plain `dent` here, since AddFile()
                                // will have replaced the file with a symlink
    };
    for (const auto& file : add_args.files) {
        try {
            std::filesystem::directory_entry dent(common_args.working_dir /
                                                  file);
            if (ignored(dent.path())) {
                // Skip.
            } else if (std::filesystem::is_directory(dent.symlink_status())) {
                for (auto it = std::filesystem::recursive_directory_iterator(
                         dent.path());
                     it != std::filesystem::recursive_directory_iterator();
                     ++it) {
                    if (ignored(it->path())) {
                        it.disable_recursion_pending();
                        continue;
                    }
                    try {
                        add_file(*it);
                    } catch (const Error& e) {
                        ++errors;
                        absl::PrintF("*** %s\n *- %s\n",
                                     pretty_path(it->path()), e.what());
                    }
                }
            } else {
                add_file(dent);
            }
        } catch (const Error& e) {
            ++errors;
            absl::PrintF("*** %s\n *- %s\n", pretty_path(file), e.what());
        }
    }

    git->Save();

    absl::PrintF(
        "\n"
        "%d files successfully added\n"
        "%d files successfully added and deduplicated\n"
        "%d directory entries skipped because they weren't regular files\n"
        "%d files skipped because of errors\n",
        successful, duplicates, nonfiles, errors);
    return errors == 0 ? 0 : 1;
}

struct FillArgs {
    std::vector<Top::ContentSource> content_sources;
};
int Fill(CommonArgs& common_args, const FillArgs& fill_args) {
    try {
        const auto result =
            common_args.top->Fill(common_args.log, common_args.working_dir,
                                  fill_args.content_sources);
        common_args.log.Important(
            "Content files\n"
            "  %d missing (restored)\n"
            "  %d missing (not restored)",
            result.num_fetched, result.num_still_missing);
        return result.num_still_missing == 0 ? 0 : 1;
    } catch (const Error& e) {
        common_args.log.Error(e.what());
        return 1;
    }
}

struct RepairArgs {
    bool fast = false;
    std::vector<Top::ContentSource> content_sources;
};
int Repair(CommonArgs& common_args, const RepairArgs& repair_args) {
    try {
        const auto result =
            common_args.top->Repair(common_args.log, common_args.working_dir,
                                    /*verify_all_hashes=*/!repair_args.fast,
                                    repair_args.content_sources);
        common_args.log.Important(
            "Index symlinks\n"
            "  %d OK\n"
            "  %d bad (removed)\n"
            "  %d missing (recreated)\n"
            "Content files\n"
            "  %d duplicates (moved aside)\n"
            "  %d missing (restored)\n"
            "  %d missing (not restored)",
            result.num_good_index_symlinks, result.num_bad_index_symlinks,
            result.num_missing_index_symlinks,
            result.num_duplicate_content_files, result.num_fetched,
            result.num_still_missing);
        return result.num_still_missing == 0 ? 0 : 1;
    } catch (const Error& e) {
        common_args.log.Error(e.what());
        return 1;
    }
}

}  // namespace

int Command(const std::filesystem::path& working_dir,
            const std::vector<std::string> args) {
    std::vector<const char*> argv = {"dummy-command-name"};
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
    }
    return Command(working_dir, argv.size(), argv.data());
}

int Command(const std::filesystem::path& working_dir, int argc,
            char const* const* const argv) {
    CLI::App app("Store files as symlinks to content-addressed storage files");
    app.require_subcommand(1);  // require exactly 1 subcommand
    app.remove_option(app.get_help_ptr());
    app.set_help_all_flag("-h,--help", "Print help message");

    CLI::App& add_command =
        *app.add_subcommand("add", "Add the given files or directories");
    AddArgs add_args;
    add_command.add_option("file", add_args.files, "Input file or directory")
        ->required()
        ->type_name("PATH");

    CLI::App& fill_command = *app.add_subcommand(
        "fill", "Look for missing content, and fill it in if possible");
    ContentSourceOptions fill_content_sources(fill_command);

    CLI::App& repair_command = *app.add_subcommand(
        "repair", "Look for damage, and fix it if possible");
    RepairArgs repair_args;
    repair_command.add_flag("--fast", repair_args.fast,
                            "Don't re-hash all content");
    ContentSourceOptions repair_content_sources(repair_command);

    CLI11_PARSE(app, argc, argv);

    const std::unique_ptr<Streamer> streamer = CreateMultiThreadedStreamer(
        {.num_buffers = 4, .bytes_per_buffer = 1024 * 1024});
    CommonArgs common_args = {
        .working_dir = working_dir,
        .log = Log(),
        .streamer = *streamer,
        .top = Top::Create(*streamer, CreateBlake3_256Hasher, "blake3")};
    if (add_command.parsed()) {
        return Add(common_args, add_args);
    } else if (fill_command.parsed()) {
        return Fill(common_args,
                    FillArgs{.content_sources =
                                 fill_content_sources.GetResult(working_dir)});
    } else if (repair_command.parsed()) {
        repair_args.content_sources =
            repair_content_sources.GetResult(working_dir);
        return Repair(common_args, repair_args);
    } else {
        FRZ_CHECK(false);
    }
}

}  // namespace frz
