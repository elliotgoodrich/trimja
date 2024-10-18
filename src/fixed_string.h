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

#include <functional>
#include <string_view>

namespace trimja {

/**
 * @class fixed_string
 * @brief A non-null terminated string with a fixed length that cannot be
 * changed after construction.
 *
 * This class is incredibly simple and will be slightly more optimal than
 * `std::string` for paths, which will overflow the small-string optimization.
 * Since `fixed_string` always allocates it is possible to create referencing
 * `std::string_view`s that will always be valid even if the `fixed_string`
 * is moved.
 *
 * Since `fixed_string` is move-only it is impossible to accidentally create
 * copies.
 */
class fixed_string {
  std::string_view m_data;

 public:
  /**
   * @brief Default constructor for fixed_string.
   */
  fixed_string();

  /**
   * @brief Constructs a fixed_string from a std::string_view.
   *
   * @param str The std::string_view to construct from.
   */
  explicit fixed_string(std::string_view str);

  /**
   * @brief Destructor for fixed_string.
   */
  ~fixed_string();

  /**
   * @brief Move construct from another fixed_string.
   *
   * `other` will be left as an empty string.
   *
   * @param other The fixed_string to move from.
   */
  fixed_string(fixed_string&& other) noexcept;

  /**
   * @brief Move assign from another fixed_string.
   *
   * `other` will be left as an empty string.
   *
   * @param rhs The fixed_string to move assign from.
   * @return A reference to this.
   */
  fixed_string& operator=(fixed_string&& rhs) noexcept;

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

/**
 * @brief Computes the hash value for a fixed_string.
 *
 * @param str The fixed_string to hash.
 * @return The hash value of the fixed_string.
 */
std::size_t hash_value(const fixed_string& str);

}  // namespace trimja

namespace std {

/**
 * @brief Specialization of std::hash for trimja::fixed_string.
 */
template <>
struct hash<trimja::fixed_string> {
  using is_transparent = void;

  /**
   * @brief Compute the hash value for a fixed_string.
   *
   * @param str The string to hash.
   * @return The hash value of the string.
   */
  size_t operator()(const string_view& str) const;
  size_t operator()(const trimja::fixed_string& str) const;
};

template <>
/**
 * @brief Functor for comparing a string_view and a fixed_string for equality.
 */
struct equal_to<trimja::fixed_string> {
  using is_transparent = void;

  /**
   * @brief Compares two strings for equality.
   * @param lhs The first string to compare.
   * @param rhs The second string to compare.
   * @return Whether the strings are equal.
   */
  bool operator()(const string_view& lhs,
                  const trimja::fixed_string& rhs) const;
  bool operator()(const trimja::fixed_string& lhs,
                  const trimja::fixed_string& rhs) const;
};

}  // namespace std

#endif  // TRIMJA_FIXED_STRING
