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

#ifndef TRIMJA_STRINGSTACK
#define TRIMJA_STRINGSTACK

#include <string>
#include <utility>
#include <vector>

namespace trimja {

/**
 * @class StringStack
 * @brief Manages a stack of `std::string`s where strings are not destroyed so
 * that their capacity can be reused.
 */
class StringStack {
  std::vector<std::string> m_stack;
  std::size_t m_realSize;

 public:
  using iterator = std::vector<std::string>::iterator;
  using const_iterator = std::vector<std::string>::const_iterator;

  /**
   * @brief Construct an empty StringStack.
   */
  StringStack();

  /**
   * @brief Return a reference to the string at the given index.
   * @pre 0 <= index < size()
   * @param index The index of the string to access.
   * @return Reference to the string at the specified index.
   */
  std::string& operator[](std::size_t index) noexcept;

  /**
   * @brief Return a const reference to the string at the given index.
   * @pre 0 <= index < size()
   * @param index The index of the string to access.
   * @return Const reference to the string at the specified index.
   */
  const std::string& operator[](std::size_t index) const noexcept;

  /**
   * @brief Add a new empty string to the top of the stack and return a
   * reference to it.
   * @return Reference to the newly added string.
   */
  std::string& emplace_back();

  /**
   * @brief Remove the string at the top of the stack.
   * @details The string is not destroyed, so its capacity can be reused.
   * @pre size() > 0
   */
  void pop() noexcept;

  /**
   * @brief Clear the stack, making it empty.
   *        The underlying storage is retained for reuse.
   */
  void clear() noexcept;

  /**
   * @brief Return an iterator to the beginning of the stack.
   * @return Iterator to the first string.
   */
  iterator begin();

  /**
   * @brief Return an iterator to the end of the stack.
   * @return Iterator to one past the last string.
   */
  iterator end();

  /**
   * @brief Return a const iterator to the beginning of the stack.
   * @return Const iterator to the first string.
   */
  const_iterator begin() const;

  /**
   * @brief Return a const iterator to the beginning of the stack.
   * @return Const iterator to the first string.
   */
  const_iterator cbegin() const;

  /**
   * @brief Return a const iterator to the end of the stack.
   * @return Const iterator to one past the last string.
   */
  const_iterator end() const;

  /**
   * @brief Return a const iterator to the end of the stack.
   * @return Const iterator to one past the last string.
   */
  const_iterator cend() const;

  /**
   * @brief Return a pointer to the underlying array of strings.
   * @return Pointer to the first string in the stack.
   */
  const std::string* data() const;

  /**
   * @brief Return the number of strings currently in the stack.
   * @return The size of the stack.
   */
  std::size_t size() const;

  /**
   * @brief Check if the stack is empty.
   * @return Return whether the stack is empty.
   */
  bool empty() const;
};

}  // namespace trimja

#endif  // TRIMJA_STRINGSTACK
