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

#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace frz {
namespace {

TEST(LogTest, Simple) {
    Log log;
    log.Info("Info message %d", 1);
    log.Important("Important message %d", 2);
    log.Error("Error message %d", 3);
    log.Progress("This will be very quick");
}

TEST(LogTest, Progress) {
    Log log;
    auto p = log.Progress("Just a moment");
    absl::SleepFor(absl::Milliseconds(1000));
}

TEST(LogTest, ProgressWithCounter) {
    Log log;
    auto p = log.Progress("Blinking");
    auto c = p.AddCounter("blinks");
    for (int i = 0; i < 1000; ++i) {
        absl::SleepFor(absl::Milliseconds(3));
        c.Increment(1);
    }
}

TEST(LogTest, ProgressWithInterruptions) {
    Log log;
    auto p = log.Progress("Some work that will be interrupted");
    absl::SleepFor(absl::Milliseconds(2000));
    log.Info("Hi there!");
    absl::SleepFor(absl::Milliseconds(2000));
    log.Info("Hi again!");
    absl::SleepFor(absl::Milliseconds(2000));
}

TEST(LogTest, ComplexProgress) {
    Log log;
    auto p = log.Progress("Counting fruit");
    auto bananas = p.AddCounter("bananas", 500);
    for (int i = 0; i < 300; ++i) {
        absl::SleepFor(absl::Milliseconds(10));
        bananas.Increment(1);
    }
    log.Info("Ooooh, found a new type of fruit!");
    auto apples = p.AddCounter("apples");
    for (int i = 0; i < 300; ++i) {
        absl::SleepFor(absl::Milliseconds(10));
        bananas.Increment(1);
        apples.Increment(2);
    }
}

TEST(LogTest, NestedProgress) {
    Log log;
    auto p1 = log.Progress("Some work");
    absl::SleepFor(absl::Milliseconds(2000));
    {
        auto p2 = log.Progress("Some other work");
        absl::SleepFor(absl::Milliseconds(2000));
        log.Important("Excuse me!");
        absl::SleepFor(absl::Milliseconds(2000));
    }
    absl::SleepFor(absl::Milliseconds(2000));
}

}  // namespace
}  // namespace frz
