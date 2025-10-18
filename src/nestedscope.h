// MIT License
//
// Copyright (c) 2025 Elliot Goodrich
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

#ifndef TRIMJA_NESTEDSCOPE
#define TRIMJA_NESTEDSCOPE

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace trimja {

class BasicScope;

/**
 * @class NestedScope
 * @brief Manages a non-empty stack of variable scopes, i.e for subninja
 * commands
 */
class NestedScope {
  std::vector<std::pair<BasicScope, std::size_t>> m_scopes;
  std::size_t m_scopeCount;
  std::size_t m_currentIndex;

 public:
  /**
   * @brief Constructs a NestedScope with 1 empty variable scope.
   */
  NestedScope();

  /**
   * @brief Return the number of scopes.
   */
  std::size_t size() const;

  /**
   * @brief Push a new scope to the top of the stack, which has all the
   * variables of the parent scope by default.
   */
  void push();

  /**
   * @brief Pop the top scope off the stack and return a BasicScope consisting
   * of all variables written to that scope.  The behavior is undefined unless
   * `size() > 1`.
   */
  [[nodiscard]] BasicScope pop();

  /**
   * @brief Sets a variable in the top scope.
   * @param key The name of the variable.
   * @param value The value of the variable.
   */
  void set(std::string_view key, std::string&& value);

  /**
   * @brief Traverse the stack of scopes and append the value of a variable to
   * the output string.
   * @param output The string to append the value to.
   * @param name The name of the variable.
   * @return Whether the variable was found.
   */
  bool appendValue(std::string& output, std::string_view name) const;
};

}  // namespace trimja

#endif  // TRIMJA_NESTEDSCOPE
