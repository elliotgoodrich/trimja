// MIT License
//
// Copyright (c) 2024 Elliot Goodrich
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef TRIMJA_RULE
#define TRIMJA_RULE

#include <ninja/eval_env.h>

#include <array>
#include <string_view>
#include <vector>

struct EvalString;

namespace trimja {

class Rule {
 public:
  inline static const std::array<std::string_view, 11> reserved = {
      "command",          "depfile", "dyndep", "description", "deps",
      "generator",        "pool",    "restat", "rspfile",     "rspfile_content",
      "msvc_deps_prefix",
  };

 private:
  std::string_view m_name;
  std::array<unsigned char, reserved.size() + 1>
      m_lookup;  // TODO: pack into uint64_t
  std::vector<EvalString> m_bindings;

 public:
  static std::size_t getLookupIndex(std::string_view varName);

  // Create a `Rule` having the specified `name`. The string pointed to by
  // `name` MUST live longer than this object.
  explicit Rule(std::string_view name);

  std::string_view name() const;

  bool add(std::string_view varName, EvalString&& value);
  bool add(std::string_view varName, const EvalString& value);

  const EvalString* lookupVar(std::string_view varName) const;
};

}  // namespace trimja

#endif