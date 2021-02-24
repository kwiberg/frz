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

#ifndef FRZ_MATH_HH_
#define FRZ_MATH_HH_

#include <concepts>

#include "assert.hh"

namespace frz {

// Return `a` rounded down to the nearest multiple of `b`.
template <std::integral T>
constexpr T RoundDown(T a, T b) {
    FRZ_ASSUME_GE(a, 0);
    FRZ_ASSUME_GT(b, 0);
    T a2 = a / b * b;
    FRZ_ASSUME_LE(a2, a);
    FRZ_ASSUME_GT(a2 + b, a);
    FRZ_ASSUME_EQ(a2 % b, 0);
    return a2;
}

// Return `a` rounded up to the nearest multiple of `b`.
template <std::integral T>
constexpr T RoundUp(T a, T b) {
    FRZ_ASSUME_GE(a, 0);
    FRZ_ASSUME_GT(b, 0);
    T a2 = (a + b - 1) / b * b;
    FRZ_ASSUME_GE(a2, a);
    FRZ_ASSUME_LT(a2, a + b);
    FRZ_ASSUME_EQ(a2 % b, 0);
    return a2;
}

}  // namespace frz

#endif  // FRZ_MATH_HH_
