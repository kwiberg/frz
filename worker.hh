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

#ifndef FRZ_WORKER_HH_
#define FRZ_WORKER_HH_

#include <absl/base/thread_annotations.h>
#include <absl/synchronization/mutex.h>
#include <functional>
#include <queue>
#include <thread>

namespace frz {

// A worker thread that accepts work items and execute them sequentially.
class Worker final {
  public:
    Worker();

    // Finishes the remaining work, and joins with the worker thread.
    ~Worker();

    // Schedule the given function to be run as soon as possible; return
    // immediately without waiting for it to run. May not be called once the
    // destructor has started.
    void Do(std::function<void()> work);

  private:
    void WorkLoop();

    absl::Mutex mutex_;
    std::queue<std::function<void()>> work_queue_ ABSL_GUARDED_BY(mutex_);
    bool quitting_ ABSL_GUARDED_BY(mutex_) = false;
    const std::jthread thread_;
};

}  // namespace frz

#endif  // FRZ_WORKER_HH_
