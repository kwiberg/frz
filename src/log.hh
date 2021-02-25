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

class ProgressLogCounter final {
  public:
    ProgressLogCounter(const ProgressLogCounter&) = delete;
    ProgressLogCounter& operator=(const ProgressLogCounter&) = delete;

    void Increment(std::int64_t diff);

  private:
    friend class ProgressLog;

    ProgressLogCounter(ProgressLog& progress_log, std::int64_t& counter)
        : progress_log_(progress_log), counter_(counter) {}

    ProgressLog& progress_log_;
    std::int64_t& counter_;
};

class ProgressLog final {
  public:
    ProgressLog(const ProgressLog&) = delete;
    ProgressLog& operator=(const ProgressLog&) = delete;
    ~ProgressLog();

    [[nodiscard]] ProgressLogCounter AddCounter(
        std::string unit,
        std::optional<std::int64_t> total_count = std::nullopt);

  private:
    friend class Log;
    friend class ProgressLogCounter;

    struct Counter {
        std::string unit;
        std::optional<std::int64_t> total_count;
        std::unique_ptr<std::int64_t> counter;
    };

    ProgressLog(std::vector<ProgressLog*>& current_progress, std::string desc);
    void PrintStatus(std::string_view new_status);
    void Render(bool done);
    void Refresh();
    void Pause();
    void Resume();

    std::vector<ProgressLog*>& in_progress_;
    const std::string desc_;
    std::vector<Counter> counters_;
    int status_characters_printed_ = 0;
    bool paused_ = true;
    bool clear_entire_line_on_pause_ = false;
    std::optional<absl::Time> last_render_;
};

class Log final {
  public:
    Log() = default;
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    ~Log() { FRZ_ASSERT_EQ(in_progress_.size(), 0); }

    void Info(std::string_view s) { OutputLine(s); }

    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) >= 1)>>
    void Info(const absl::FormatSpec<Ts...>& format, Ts... args) {
        OutputLine(absl::StrFormat(format, std::forward<Ts>(args)...));
    }

    void Important(std::string_view s) { OutputLine(s); }

    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) >= 1)>>
    void Important(const absl::FormatSpec<Ts...>& format, Ts... args) {
        OutputLine(absl::StrFormat(format, std::forward<Ts>(args)...));
    }

    void Error(std::string_view s) {
        OutputLine(absl::StrCat("*** ERROR: ", s));
    }

    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) >= 1)>>
    void Error(const absl::FormatSpec<Ts...>& format, Ts... args) {
        OutputLine(absl::StrCat(
            "*** ERROR: ", absl::StrFormat(format, std::forward<Ts>(args)...)));
    }

    ProgressLog Progress(std::string_view s) {
        return StartProgress(absl::StrCat(s, "... "));
    }

    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) >= 1)>>
    ProgressLog Progress(const absl::FormatSpec<Ts...>& format, Ts... args) {
        return StartProgress(absl::StrCat(
            absl::StrFormat(format, std::forward<Ts>(args)...), "... "));
    }

  private:
    void OutputLine(std::string_view line);
    [[nodiscard]] ProgressLog StartProgress(std::string desc);

    std::vector<ProgressLog*> in_progress_;
};

}  // namespace frz

#endif  // FRZ_LOG_HH_
