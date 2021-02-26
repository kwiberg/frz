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

#include "content_store.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "filesystem_testing.hh"

namespace frz {
namespace {

using ::testing::Eq;
using ::testing::Optional;

TEST(TestContentStore, CanonicalPath) {
    TempDir d;
    d.Dir("cs");
    std::unique_ptr<ContentStore> cs = ContentStore::Create(d.Path() / "cs");
    EXPECT_THAT(cs->CanonicalPath("/foo/bar"), Eq(std::nullopt));
    EXPECT_THAT(cs->CanonicalPath(d.Path() / "foo"), Eq(std::nullopt));
    EXPECT_THAT(cs->CanonicalPath(d.Path() / "cs"), Optional(Eq(".")));
    EXPECT_THAT(cs->CanonicalPath(d.Path() / "cs/.."), Eq(std::nullopt));
    EXPECT_THAT(cs->CanonicalPath(d.Path() / "foo/bar/../../cs/baz/kk"),
                Optional(Eq("baz/kk")));
}

}  // namespace
}  // namespace frz
