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
  std::string& resetValue(std::string_view key);
};

}  // namespace detail

/**
 * @class EdgeScope
 * @brief Manages the scope of variables for a specific build edge.
 *
 * The EdgeScope class is responsible for handling variable lookups and
 * substitutions within the context of a specific build edge. It extends
 * the functionality of EdgeScopeBase and interacts with a parent scope.
 *
 * @tparam SCOPE The type of the parent scope.
 */
template <typename SCOPE>
class EdgeScope : private detail::EdgeScopeBase {
  SCOPE& m_parent;

 public:
  /**
   * @brief Constructs an EdgeScope with the given parent scope, rule, inputs,
   * and outputs.
   * @param parent The parent scope.
   * @param rule The rule associated with the build edge.
   * @param ins The input files for the build edge.
   * @param outs The output files for the build edge.
   */
  EdgeScope(SCOPE& parent,
            const Rule& rule,
            std::span<const std::string> ins,
            std::span<const std::string> outs)
      : detail::EdgeScopeBase(rule, ins, outs), m_parent(parent) {}

  /**
   * @brief Sets a local variable in the edge scope.
   *
   * This method sets a local variable with the given key and value within the
   * edge scope. The variable is stored in the local scope and can be used for
   * variable substitution during the build process.
   *
   * @param key The name of the variable to set.
   * @param value The value to assign to the variable.
   * @return A string view of the stored value.
   */
  using detail::EdgeScopeBase::set;

  /**
   * @brief Resets the value of a variable in the scope.
   *
   * This method sets the value associated with the specified key to an empty
   * string. If the key does not exist, it inserts the key with an empty value.
   *
   * @param key The name of the variable to reset.
   * @return A reference to the reset value.
   */
  using detail::EdgeScopeBase::resetValue;

  /**
   * @brief Appends the value of a variable to the output string.
   *
   * This method looks up the value of the specified variable name and appends
   * it to the output string. The lookup follows the order of precedence as
   * described in the Ninja build manual.
   *
   * @param output The string to append the value to.
   * @param name The name of the variable.
   * @return Whether the variable was found in this scope.
   */
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

#endif  // TRIMJA_EDGESCOPE
