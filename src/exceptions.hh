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

#ifndef FRZ_EXCEPTIONS_HH_
#define FRZ_EXCEPTIONS_HH_

#include <absl/strings/str_format.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace frz {

// Application specific exception. We convert all other exceptions to this if
// we intend to propagate them up the stack.
class Error final {
  public:
    Error(std::string what) : what_(std::move(what)) {}

    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) >= 1)>>
    Error(const absl::FormatSpec<Ts...>& format, Ts... args)
        : what_(absl::StrFormat(format, std::forward<Ts>(args)...)) {}

    std::string_view what() const noexcept { return what_; }

  private:
    std::string what_;
};

}  // namespace frz

#endif  // FRZ_EXCEPTIONS_HH_
