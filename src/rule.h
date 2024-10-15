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

#include <string_view>
#include <utility>
#include <vector>

namespace trimja {

class EvalString;

/**
 * @class Rule
 * @brief Represents a build rule in the Ninja build system.
 */
class Rule {
 public:
  /**
   * @brief List of reserved variable names in a rule.
   */
  inline static const std::string_view reserved[] = {
      "command",          "depfile", "dyndep", "description", "deps",
      "generator",        "pool",    "restat", "rspfile",     "rspfile_content",
      "msvc_deps_prefix",
  };

 private:
  std::string_view m_name;

  // The std::string_view* points to an element of reserved
  std::vector<std::pair<const std::string_view*, EvalString>> m_bindings;

 public:
  /**
   * @brief Gets the lookup index for a given variable name.
   *
   * @param varName The variable name to look up.
   * @return The index of the variable name in the reserved list.
   */
  static std::size_t getLookupIndex(std::string_view varName);

  /**
   * @brief Constructs a Rule with the given name.
   *
   * @param name The name of the rule. The lifetime of the string pointed to by
   * name must outlive this object.
   */
  explicit Rule(std::string_view name);

  /**
   * @brief Gets the name of the rule.
   *
   * @return The name of the rule.
   */
  std::string_view name() const;

  /**
   * @brief Adds a variable and its value to the rule.
   *
   * @param varName The name of the variable.
   * @param value The value of the variable.
   * @return True if the variable was added successfully, false otherwise.
   */
  bool add(std::string_view varName, EvalString value);

  /**
   * @brief Looks up the value of a variable in the rule.
   *
   * @param varName The name of the variable to look up.
   * @return A pointer to the EvalString value of the variable, or nullptr if
   * not found.
   */
  const EvalString* lookupVar(std::string_view varName) const;
};

}  // namespace trimja

#endif
