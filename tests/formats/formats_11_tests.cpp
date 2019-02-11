////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "formats_test_case_base.hpp"
#include "store/directory_attributes.hpp"

NS_LOCAL

using tests::format_test_case;

INSTANTIATE_TEST_CASE_P(
  format_11_test,
  format_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::rot13_cipher_directory<&tests::memory_directory, 16>,
      &tests::rot13_cipher_directory<&tests::fs_directory, 16>,
      &tests::rot13_cipher_directory<&tests::mmap_directory, 16>
    ),
    ::testing::Values("1_1")
  ),
  tests::to_string
);

NS_END