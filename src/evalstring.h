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

#ifndef TRIMJA_EVALSTRING
#define TRIMJA_EVALSTRING

#include <cassert>
#include <string>
#include <string_view>

namespace trimja {

/**
 * @class EvalString
 * @brief A class to represent and evaluate strings containing both text and
 * variables.
 */
class EvalString {
 public:
  // For the moment use `std::size_t` as offset type but we can investigate
  // later on if a smaller type help with memory or performance.
  using Offset = std::size_t;

 private:
  std::string m_data;
  Offset m_lastSegmentLength;

 public:
  /**
   * @enum TokenType
   * @brief Enum to represent the type of token.
   */
  enum class TokenType {
    Text,     ///< Represents a text token.
    Variable  ///< Represents a variable token.
  };

  /**
   * @class const_iterator
   * @brief A constant iterator to iterate over the tokens in EvalString.
   */
  class const_iterator {
    const char* m_pos;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<std::string_view, TokenType>;

    const_iterator();
    const_iterator(const char* pos);
    std::pair<std::string_view, TokenType> operator*() const;
    const_iterator& operator++();
    const_iterator operator++(int);
    friend bool operator==(const_iterator lhs, const_iterator rhs);
    friend bool operator!=(const_iterator lhs, const_iterator rhs);
  };

  /**
   * @brief Default constructor.
   */
  EvalString();

  /**
   * @brief Clears the EvalString.
   */
  void clear();

  /**
   * @brief Checks if the EvalString is empty.
   */
  bool empty() const;

  /**
   * @brief Gets the beginning iterator.
   */
  const_iterator begin() const;

  /**
   * @brief Gets the end iterator.
   */
  const_iterator end() const;

  /**
   * @brief Appends text to the EvalString, consolidating consecutive text
   * sections.
   * @param text The text to append.
   */
  void appendText(std::string_view text);

  /**
   * @brief Appends a variable to the EvalString.
   * @param name The name of the variable to append.
   */
  void appendVariable(std::string_view name);
};

/**
 * @brief Evaluates the EvalString and appends the result to the output.
 * @tparam SCOPE The type of the scope.
 * @param output The output string to append the result to.
 * @param variable The EvalString to evaluate.
 * @param scope The scope to use for variable evaluation.
 */
template <typename SCOPE>
void evaluate(std::string& output,
              const EvalString& variable,
              const SCOPE& scope) {
  const auto end = variable.end();
  for (auto it = variable.begin(); it != end; ++it) {
    switch ((*it).second) {
      case EvalString::TokenType::Text:
        output += (*it).first;
        if (++it == end) {
          return;
        }
        // As we consolidate consecutive text sections if we are not at the
        // end then we must have a variable next
        assert((*it).second == EvalString::TokenType::Variable);
        [[fallthrough]];
      case EvalString::TokenType::Variable:
        scope.appendValue(output, (*it).first);
    }
  }
}

}  // namespace trimja

#endif  // TRIMJA_EVALSTRING
