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
   * @brief Sets a variable in the scope.
   * @param key The name of the variable.
   * @param value The value of the variable.
   * @return A reference to the inserted value.
   */
  std::string_view set(std::string_view key, std::string&& value);

  /**
   * @brief Resets the value of a variable in the scope.
   *
   * This method sets the value associated with the specified key to an empty
   * string. If the key does not exist, it inserts the key with an empty value.
   *
   * @param key The name of the variable to reset.
   * @return A reference to the reset value.
   */
  std::string& resetValue(std::string_view key);

  /**
   * @brief Appends the value of a variable to the output string.
   * @param output The string to append the value to.
   * @param name The name of the variable.
   * @return Whether the variable was found in this scope.
   */
  bool appendValue(std::string& output, std::string_view name) const;
};

}  // namespace trimja

#endif  // TRIMJA_BASICSCOPE