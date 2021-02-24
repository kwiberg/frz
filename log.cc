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

#include "log.hh"

#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assert.hh"

namespace frz {

namespace {

void MoveCursorLeft(int steps) {
    if (steps > 0) {
        absl::PrintF("\033[%dD", steps);
    }
}

}  // namespace

void ProgressLogCounter::Increment(std::int64_t diff) {
    counter_ += diff;
    progress_log_.Refresh();
}

ProgressLog::~ProgressLog() {
    Render(true);
    std::putchar('\n');
    FRZ_ASSERT_EQ(in_progress_.back(), this);
    in_progress_.pop_back();
    if (!in_progress_.empty()) {
        in_progress_.back()->Resume();
    }
}

ProgressLogCounter ProgressLog::AddCounter(
    std::string unit, std::optional<std::int64_t> total_count) {
    counters_.push_back(Counter{.unit = std::move(unit),
                                .total_count = total_count,
                                .counter = std::make_unique<std::int64_t>(0)});
    return ProgressLogCounter(*this, *counters_.back().counter);
}

ProgressLog::ProgressLog(std::vector<ProgressLog*>& in_progress,
                         std::string desc)
    : in_progress_(in_progress), desc_(std::move(desc)) {
    if (!in_progress.empty()) {
        in_progress.back()->Pause();
    }
    in_progress.push_back(this);
    Resume();
}

void ProgressLog::PrintStatus(std::string_view new_status) {
    MoveCursorLeft(status_characters_printed_);
    absl::PrintF("%-*s", status_characters_printed_, new_status);
    MoveCursorLeft(status_characters_printed_ - std::ssize(new_status));
    status_characters_printed_ = std::ssize(new_status);
    std::fflush(stdout);
}

void ProgressLog::Render(bool done) {
    FRZ_ASSERT(!paused_);
    const absl::Time now = absl::Now();
    if (!done && last_render_.has_value() &&
        now - *last_render_ < absl::Milliseconds(100)) {
        return;
    }
    last_render_ = now;
    if (counters_.empty()) {
        PrintStatus(done ? "done" : "");
        return;
    }
    const std::string counters =
        absl::StrJoin(counters_, ", ", [](std::string* out, const Counter& c) {
            absl::StrAppendFormat(out, "%d %s", *c.counter, c.unit);
        });
    const std::string percent_or_done = [&]() -> std::string {
        if (done) {
            return "done";
        }
        for (const auto& c : counters_) {
            if (c.total_count.has_value()) {
                return absl::StrFormat("%.2f%%",
                                       100.0 * *c.counter / *c.total_count);
            }
        }
        return "";
    }();
    PrintStatus(percent_or_done.empty()
                    ? counters
                    : absl::StrFormat("%s (%s)", percent_or_done, counters));
}

void ProgressLog::Refresh() {
    if (!paused_) {
        Render(false);
    }
}

void ProgressLog::Pause() {
    FRZ_ASSERT(!paused_);
    if (clear_entire_line_on_pause_) {
        // We've been paused at least once before, so this time we'll clear the
        // entire line.
        const int total_characters_printed =
            std::ssize(desc_) + status_characters_printed_;
        MoveCursorLeft(total_characters_printed);
        absl::PrintF("%*s", total_characters_printed, "");
        MoveCursorLeft(total_characters_printed);
        status_characters_printed_ = 0;
    } else {
        // This is the first time we've been paused. Clear the status, but
        // leave the description and print a newline.
        clear_entire_line_on_pause_ = true;
        PrintStatus("");
        std::putchar('\n');
    }
    FRZ_ASSERT_EQ(status_characters_printed_, 0);
    paused_ = true;
    last_render_ = std::nullopt;
}

void ProgressLog::Resume() {
    FRZ_ASSERT(paused_);
    FRZ_ASSERT(!last_render_.has_value());
    FRZ_ASSERT_EQ(status_characters_printed_, 0);
    FRZ_ASSERT_GE(in_progress_.size(), 1);
    FRZ_ASSERT_EQ(in_progress_.back(), this);
    absl::PrintF("%*s%s", (in_progress_.size() - 1) * 2, "", desc_);
    paused_ = false;
    Render(false);
    FRZ_ASSERT(last_render_.has_value());
}

void Log::OutputLine(std::string_view line) {
    if (!in_progress_.empty()) {
        in_progress_.back()->Pause();
    }
    absl::PrintF("%*s%s\n", in_progress_.size() * 2, "", line);
    if (!in_progress_.empty()) {
        in_progress_.back()->Resume();
    }
}

ProgressLog Log::StartProgress(std::string desc) {
    return ProgressLog(in_progress_, std::move(desc));
}

}  // namespace frz
