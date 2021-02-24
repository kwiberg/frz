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

#ifndef FRZ_NETTLE_SHA256_HASHER_HH_
#define FRZ_NETTLE_SHA256_HASHER_HH_

#include <memory>

#include "hasher.hh"

namespace frz {

std::unique_ptr<Hasher<256>> CreateNettleSha256Hasher();

}  // namespace frz

#endif  // FRZ_NETTLE_SHA256_HASHER_HH_
