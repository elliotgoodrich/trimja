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
//   * Remove everything that isn't `EvalString`
//   * Remove `EvalString::Evaluate`
//   * Replace `StringPiece` with `std::string_view`
//   * Add `EvalString() = default`
//   * Make everything public
//   * Add 'evaluate' taking a template 'SCOPE'

#ifndef NINJA_EVAL_ENV_H_
#define NINJA_EVAL_ENV_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// A tokenized string that contains variable references.
/// Can be evaluated relative to an Env.
struct EvalString {
  EvalString();

  /// @return The string with variables not expanded.
  std::string Unparse() const;

  void Clear() { parsed_.clear(); }
  bool empty() const { return parsed_.empty(); }

  void AddText(std::string_view text);
  void AddSpecial(std::string_view text);

  /// Construct a human-readable representation of the parsed state,
  /// for use in tests.
  std::string Serialize() const;

  enum TokenType { RAW, SPECIAL };
  typedef std::vector<std::pair<std::string, TokenType> > TokenList;
  TokenList parsed_;
};

template <typename SCOPE>
void evaluate(std::string& output,
              const EvalString& variable,
              const SCOPE& scope) {
  for (const auto& [string, type] : variable.parsed_) {
    if (type == EvalString::RAW) {
      output += string;
    } else {
      scope.appendValue(output, string);
    }
  }
}


#endif  // NINJA_EVAL_ENV_H_
