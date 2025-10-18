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

#ifndef TRIMJA_BASICSCOPE
#define TRIMJA_BASICSCOPE

#include "fixed_string.h"

#include <boost/boost_unordered.hpp>

#include <string>
#include <string_view>

namespace trimja {

/**
 * @class BasicScope
 * @brief Manages a scope of variables for evaluation and substitution.
 */
class BasicScope {
  boost::unordered_flat_map<fixed_string,
                            std::string,
                            std::hash<trimja::fixed_string>>
      m_variables;

 public:
  /**
   * @brief Constructs an empty BasicScope.
   */
  BasicScope();

  /**
   * @brief Move constructor.
   * @param other The BasicScope to move from.
   */
  BasicScope(BasicScope&& other) noexcept;

  /**
   * @brief Copy constructor.
   * @param other The BasicScope to copy from.
   */
  BasicScope(const BasicScope& other);

  /**
   * @brief Move assignment operator.
   * @param rhs The BasicScope to move assign from.
   * @return A reference to the assigned BasicScope.
   */
  BasicScope& operator=(BasicScope&& rhs) noexcept;

  /**
   * @brief Copy assignment operator.
   * @param rhs The BasicScope to copy assign from.
   * @return A reference to the assigned BasicScope.
   */
  BasicScope& operator=(const BasicScope& rhs);

  /**
   * @brief For each variable in this scope, if the value is the same as in the
   * parent scope, remove it from this scope. Otherwise, set its value to that
   * in the parent scope.
   * @param parent The parent scope to revert against.
   * @return A reference to this newly modified BasicScope.
   */
  template <typename SCOPE>
  BasicScope& revert(const SCOPE& parent);

  /**
   * @brief Sets a variable in the scope.
   * @param key The name of the variable.
   * @param value The value of the variable.
   */
  void set(std::string_view key, std::string&& value);

  /**
   * @brief Appends the value of a variable to the output string.
   * @param output The string to append the value to.
   * @param name The name of the variable.
   * @return Whether the variable was found in this scope.
   */
  bool appendValue(std::string& output, std::string_view name) const;

  /**
   * @brief Returns an iterator to the beginning of the variables.
   * @return An iterator to the beginning of the variables.
   */
  auto begin() const { return m_variables.cbegin(); }

  /**
   * @brief Returns an iterator to the end of the variables.
   * @return An iterator to the end of the variables.
   */
  auto end() const { return m_variables.cend(); }

  /**
   * @brief Swap the values of the parameters.
   * @param lhs The first value to swap with the second
   * @param rhs The second value to swap with the first
   */
  friend void swap(BasicScope& lhs, BasicScope& rhs) noexcept;
};

template <typename SCOPE>
BasicScope& BasicScope::revert(const SCOPE& parent) {
  std::string parentValue;
  for (auto it = m_variables.begin(), last = m_variables.end(); it != last;) {
    auto& [name, value] = *it;
    parentValue.clear();
    parent.appendValue(parentValue, name);
    if (value != parentValue) {
      value = parentValue;
      ++it;
    } else {
      it = m_variables.erase(it);
    }
  }
  return *this;
}

}  // namespace trimja

#endif  // TRIMJA_BASICSCOPE
