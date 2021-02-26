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

#ifndef FRZ_LOG_HH_
#define FRZ_LOG_HH_

#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <absl/time/time.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "assert.hh"

namespace frz {

class ProgressLog;

// Object that represents a counter for the the currently ongoing operation;
// see ProgressLog::AddCounter.
class ProgressLogCounter final {
  public:
    ProgressLogCounter(const ProgressLogCounter&) = delete;
    ProgressLogCounter& operator=(const ProgressLogCounter&) = delete;

    // Increment the counter. This is a very cheap operation, since it will not
    // always cause the displayed output to be updated.
    void Increment(std::int64_t diff);

  private:
    friend class ProgressLog;

    ProgressLogCounter(ProgressLog& progress_log, std::int64_t& counter)
        : progress_log_(progress_log), counter_(counter) {}

    ProgressLog& progress_log_;
    std::int64_t& counter_;
};

// Object that represents a currently ongoing operation; see Log::Progress. The
// operation is ended when this object is destroyed.
class ProgressLog final {
  public:
    ProgressLog(const ProgressLog&) = delete;
    ProgressLog& operator=(const ProgressLog&) = delete;
    ~ProgressLog();

    // Add a counter for something, which will be displayed along with the
    // general progress of the operation; `unit` might be e.g. "files",
    // "bytes", or whatever you're counting. If `total_count` is not nullopt, a
    // "percentage done" value will also be displayed. The returned
    // ProgressLogCounter object can be used to actually increment the counter
    // value.
    [[nodiscard]] ProgressLogCounter AddCounter(
        std::string unit,
        std::optional<std::int64_t> total_count = std::nullopt);

  private:
    friend class Log;
    friend class ProgressLogCounter;

    // Per-counter data. `counter` is stored in a separate heap allocation
    // because we need it to have a stable address.
    struct Counter {
        std::string unit;
        std::optional<std::int64_t> total_count;
        std::unique_ptr<std::int64_t> counter;
    };

    ProgressLog(std::vector<ProgressLog*>& current_progress, std::string desc);

    // Erase the current status string and print `new_status` instead.
    void PrintStatus(std::string_view new_status);

    // Update the status string (unless not enough time has passed since the
    // last update). `done` is true if the operation has finished.
    void Render(bool done);

    // Update the status string, unless we're paused or not enough time has
    // passed since the last update.
    void Refresh();

    // Go in to/out of the paused state.
    void Pause();
    void Resume();

    // The stack of live ProgressLogs. We add ourself to this when we're
    // created, and remove ourself when we're destroyed.
    std::vector<ProgressLog*>& in_progress_;

    const std::string desc_;
    std::vector<Counter> counters_;

    // How many status characters have we printed? (This is the stuff after the
    // "... ".)
    int status_characters_printed_ = 0;

    // Are we currently paused? (This happens briefly if a log line needs to be
    // printed while we run, or for a longer period if another Log::Progress
    // call is made.)
    bool paused_ = true;

    bool clear_entire_line_on_pause_ = false;

    // Last time we updated the display, if any.
    std::optional<absl::Time> last_render_;
};

// An object that can print to the console. Not copyable or movable; the idea
// is that you create one, and then pass it by reference to places that need to
// do logging.
class Log final {
  public:
    Log() = default;
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    ~Log() { FRZ_ASSERT_EQ(in_progress_.size(), 0); }

    // Log a string, at various levels of urgency.
    void Info(std::string_view s) { OutputLine(s); }
    void Important(std::string_view s) { OutputLine(s); }
    void Error(std::string_view s) {
        OutputLine(absl::StrCat("*** ERROR: ", s));
    }

    // Start an operation (described by `s`) that may take some time; the
    // returned ProgressLog will log that the operation is finished in its
    // destructor. For example, .Progress("Counting sheep") might cause
    // "Counting sheep..." to be output, changing to "Counting sheep... done"
    // when the ProgressLog object is destroyed.
    [[nodiscard]] ProgressLog Progress(std::string_view s) {
        return StartProgress(absl::StrCat(s, "... "));
    }

    // Convenience overloads of the above that accept the same arguments as
    // absl::StrFormat, saving callers from having to use absl::StrFormat
    // themselves.
    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) >= 1)>>
    void Info(const absl::FormatSpec<Ts...>& format, Ts... args) {
        OutputLine(absl::StrFormat(format, std::forward<Ts>(args)...));
    }
    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) >= 1)>>
    void Important(const absl::FormatSpec<Ts...>& format, Ts... args) {
        OutputLine(absl::StrFormat(format, std::forward<Ts>(args)...));
    }
    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) >= 1)>>
    void Error(const absl::FormatSpec<Ts...>& format, Ts... args) {
        OutputLine(absl::StrCat(
            "*** ERROR: ", absl::StrFormat(format, std::forward<Ts>(args)...)));
    }
    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) >= 1)>>
    [[nodiscard]] ProgressLog Progress(const absl::FormatSpec<Ts...>& format,
                                       Ts... args) {
        return StartProgress(absl::StrCat(
            absl::StrFormat(format, std::forward<Ts>(args)...), "... "));
    }

  private:
    void OutputLine(std::string_view line);
    [[nodiscard]] ProgressLog StartProgress(std::string desc);

    // Stack of the currently ongoing ProgressLogs.
    std::vector<ProgressLog*> in_progress_;
};

}  // namespace frz

#endif  // FRZ_LOG_HH_
