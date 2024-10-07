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

#ifndef TRIMJA_FIXED_STRING
#define TRIMJA_FIXED_STRING

#include <string_view>

namespace trimja {

/**
 * @class fixed_string
 * @brief A non-null terminated string with a fixed length that cannot be
 * changed after construction.
 *
 * This class is useful as the key to an associative container since these are
 * immutable after construction. It has no small-string optimization (yet).
 */
class fixed_string {
  std::string_view m_data;

 public:
  /**
   * @struct copy_from_string_view_t
   * @brief Helper struct to create a fixed_string from a std::string_view.
   */
  struct copy_from_string_view_t {
    std::string_view data;
  };

  /**
   * @brief Create a temporary const fixed_string reference.
   *
   * The lifetime of the returned reference is the same as the lifetime of
   * the std::string_view.
   *
   * @param str The string view to create the fixed_string from.
   * @return A const reference to the created fixed_string.
   */
  static const fixed_string& make_temp(const std::string_view& str) noexcept;

  /**
   * @brief Create an object that can construct a fixed_string from a string.
   *
   * @param str The string view to create the fixed_string from.
   * @return A copy_from_string_view_t object containing the string view.
   */
  static copy_from_string_view_t create(const std::string_view& str);

  /**
   * @brief Construct a fixed_string from a string.
   *
   * We do not have a constructor from std::string_view in case users forget
   * to call make_temp when looking up values and we construct a temporary
   * fixed_string.
   *
   * @param str The copy_from_string_view_t object containing the string view.
   */
  fixed_string(const copy_from_string_view_t& str);

  ~fixed_string();

  fixed_string(const fixed_string&) = delete;
  fixed_string& operator=(const fixed_string&) = delete;

  /**
   * @brief Implicit conversion operator to std::string_view.
   *
   * @return A std::string_view representing the fixed_string.
   */
  operator std::string_view() const;

  /**
   * @brief Get a std::string_view of the fixed_string.
   *
   * @return A std::string_view representing the fixed_string.
   */
  std::string_view view() const;
};

/**
 * @brief Equality operator for fixed_string.
 *
 * @param lhs The left-hand side fixed_string.
 * @param rhs The right-hand side fixed_string.
 * @return Whether the fixed_strings are equal.
 */
bool operator==(const fixed_string& lhs, const fixed_string& rhs);

/**
 * @brief Inequality operator for fixed_string.
 *
 * @param lhs The left-hand side fixed_string.
 * @param rhs The right-hand side fixed_string.
 * @return Whether the fixed_strings are not equal.
 */
bool operator!=(const fixed_string& lhs, const fixed_string& rhs);

}  // namespace trimja

namespace std {

/**
 * @brief Specialization of std::hash for trimja::fixed_string.
 */
template <>
struct hash<trimja::fixed_string> {
  /**
   * @brief Compute the hash value for a fixed_string.
   *
   * @param str The fixed_string to hash.
   * @return The hash value of the fixed_string.
   */
  size_t operator()(const trimja::fixed_string& str) const {
    return hash<std::string_view>{}(str.view());
  }
};

}  // namespace std

#endif  // TRIMJA_FIXED_STRING
