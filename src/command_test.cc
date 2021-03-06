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

#include "command.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "filesystem_testing.hh"

namespace frz {
namespace {

TEST(TestCommand, NoSubcommandIsError) {
    TempDir d;
    EXPECT_NE(0, Command(d.Path(), {}));
}

TEST(TestCommand, NoSubcommandWithHelpIsOk) {
    TempDir d;
    EXPECT_EQ(0, Command(d.Path(), {"-h"}));
    EXPECT_EQ(0, Command(d.Path(), {"--help"}));
}

}  // namespace
}  // namespace frz
