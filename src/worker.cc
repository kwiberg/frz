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

#include "worker.hh"

#include <absl/synchronization/mutex.h>
#include <functional>

#include "assert.hh"

namespace frz {

Worker::Worker() : thread_([this] { WorkLoop(); }) {}

Worker::~Worker() {
    absl::MutexLock ml(&mutex_);
    FRZ_ASSERT(!quitting_);
    quitting_ = true;
}

void Worker::Do(std::function<void()> work) {
    absl::MutexLock ml(&mutex_);
    FRZ_ASSERT(!quitting_);
    work_queue_.push(std::move(work));
}

void Worker::WorkLoop() {
    while (true) {
        std::function<void()> work;
        {
            auto not_blocked = [&] {
                return quitting_ || !work_queue_.empty();
            };
            absl::MutexLock ml(&mutex_, absl::Condition(&not_blocked));
            if (work_queue_.empty()) {
                FRZ_ASSERT(quitting_);
                return;
            } else {
                work = std::move(work_queue_.front());
                work_queue_.pop();
            }
        }
        work();
    }
}

}  // namespace frz
