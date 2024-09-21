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

#ifndef TRIMJA_EDGESCOPE
#define TRIMJA_EDGESCOPE

#include "basicscope.h"
#include "rule.h"

#include <ninja/eval_env.h>

#include <span>
#include <string>
#include <string_view>

namespace trimja {

namespace detail {

class EdgeScopeBase {
 protected:
  std::span<const std::string> m_ins;
  std::span<const std::string> m_outs;
  BasicScope m_local;
  const Rule& m_rule;

  static void appendPaths(std::string& output,
                          std::span<const std::string> paths,
                          const char separator);

 public:
  EdgeScopeBase(const Rule& rule,
                std::span<const std::string> ins,
                std::span<const std::string> outs);

  std::string_view set(std::string_view key, std::string&& value);
};

}  // namespace detail

template <typename SCOPE>
class EdgeScope : private detail::EdgeScopeBase {
  SCOPE& m_parent;

 public:
  EdgeScope(SCOPE& parent,
            const Rule& rule,
            std::span<const std::string> ins,
            std::span<const std::string> outs)
      : detail::EdgeScopeBase(rule, ins, outs), m_parent(parent) {}

  using detail::EdgeScopeBase::set;

  bool appendValue(std::string& output, std::string_view name) const {
    // From https://ninja-build.org/manual.html#ref_scope
    // Variable declarations indented in a build block are scoped to the build
    // block. The full lookup order for a variable expanded in a build block (or
    // the rule is uses) is:
    //   1. Special built-in variables ($in, $out).
    //   2. Build-level variables from the build block.
    //   3. Rule-level variables from the rule block (i.e. $command). (Note from
    //      the above discussion on expansion that these are expanded "late",
    //      and may make use of in-scope bindings like $in.)
    //   4. File-level variables from the file that the build line was in.
    //   5. Variables from the file that included that file using the subninja
    //      keyword.
    if (name == "in") {
      appendPaths(output, m_ins, ' ');
      return true;
    } else if (name == "out") {
      appendPaths(output, m_outs, ' ');
      return true;
    } else if (name == "in_newline") {
      appendPaths(output, m_ins, '\n');
      return true;
    } else if (m_local.appendValue(output, name)) {
      return true;
    } else if (const EvalString* value = m_rule.lookupVar(name)) {
      evaluate(output, *value, *this);
      return true;
    } else {
      return m_parent.appendValue(output, name);
    }
  }
};

}  // namespace trimja

#endif