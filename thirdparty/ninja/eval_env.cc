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

#include "eval_env.h"

EvalString::EvalString() = default;

void EvalString::AddText(std::string_view text) {
  // Add it to the end of an existing RAW token if possible.
  if (!parsed_.empty() && parsed_.back().second == RAW) {
    parsed_.back().first += text;
  } else {
    parsed_.emplace_back(text, RAW);
  }
}
void EvalString::AddSpecial(std::string_view text) {
  parsed_.emplace_back(text, SPECIAL);
}

std::string EvalString::Serialize() const {
  std::string result;
  for (TokenList::const_iterator i = parsed_.begin();
       i != parsed_.end(); ++i) {
    result.append("[");
    if (i->second == SPECIAL)
      result.append("$");
    result.append(i->first);
    result.append("]");
  }
  return result;
}

std::string EvalString::Unparse() const {
  std::string result;
  for (TokenList::const_iterator i = parsed_.begin();
       i != parsed_.end(); ++i) {
    bool special = (i->second == SPECIAL);
    if (special)
      result.append("${");
    result.append(i->first);
    if (special)
      result.append("}");
  }
  return result;
}
