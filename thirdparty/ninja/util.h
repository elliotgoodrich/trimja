// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modifications
// -------------
// The following modifications have been made to `lexer.h` and `lexer.cc`,
// they are specified here in accordance with the Apache License 2.0
// requirement.  The modifications themselves are also released under
// Apache License 2.0.
//
//   * Removed everything except `CanonicalizePath`,
//     `GetShellEscapedString`, and `GetWin32EscapedString`
//   * Moved `static` functions into an unnamed namespace
//   * Removed `using namespace std;` and added back in namespace
//     qualifier where appropriate
//   * Replaced both escaping strings with one `GetWin32EscapedString`
//     function (since it will keep the ifdefs to a minimum in other bits
//     of code and we're not unit testing - only system tests)
//   * Update some of the code to use `std::all_of`
//   * Use `std::string_view` where possible

#ifndef NINJA_UTIL_H_
#define NINJA_UTIL_H_

#include <cstdint>
#include <string>

/// Canonicalize a path like "foo/../bar.h" into just "bar.h".
/// |slash_bits| has bits set starting from lowest for a backslash that was
/// normalized to a forward slash. (only used on Windows)
void CanonicalizePath(std::string* path, std::uint64_t* slash_bits);
void CanonicalizePath(char* path, size_t* len, std::uint64_t* slash_bits);

/// Appends |input| to |*result|, escaping according to the whims of either
/// Bash, or Win32's CommandLineToArgvW().
/// Appends the string directly to |result| without modification if we can
/// determine that it contains no problematic characters.
void appendEscapedString(std::string& output, std::string_view input);

#endif  // NINJA_UTIL_H_
