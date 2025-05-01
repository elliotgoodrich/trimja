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

#ifndef TRIMJA_NINJA_CLOCK
#define TRIMJA_NINJA_CLOCK

#include <chrono>

namespace trimja {

/**
 * @struct ninja_clock
 * @brief Represents the on-disk time saved within the .ninja_deps file.
 *
 * ninja_clock::time_point is guaranteed to be the same layout
 * as the last modified time saved in this file.
 */
struct ninja_clock {
  using rep = std::uint64_t;
  using period = std::nano;
  using duration = std::chrono::duration<rep, period>;
  using time_point = std::chrono::time_point<ninja_clock>;
  static const bool is_steady = false;

  /**
   * @brief Converts a file_clock::time_point to a ninja_clock::time_point.
   *
   * @param t The file_clock::time_point to convert.
   * @return The corresponding ninja_clock::time_point.
   */
  static trimja::ninja_clock::time_point from_file_clock(
      std::chrono::file_clock::time_point t);

  /**
   * @brief Converts a ninja_clock::time_point to a file_clock::time_point.
   *
   * @param t The ninja_clock::time_point to convert.
   * @return The corresponding file_clock::time_point.
   */
  static std::chrono::file_clock::time_point to_file_clock(
      trimja::ninja_clock::time_point t);
};

}  // namespace trimja

#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 202002L
// Supported only with later versions of C++20
namespace std {
namespace chrono {

/**
 * @struct clock_time_conversion
 * @brief Specialization for converting between trimja::ninja_clock and
 * file_clock.
 */
template <>
struct clock_time_conversion<trimja::ninja_clock, file_clock> {
  /**
   * @brief Converts a file_clock::time_point to a ninja_clock::time_point.
   *
   * @param t The file_clock::time_point to convert.
   * @return The corresponding ninja_clock::time_point.
   */
  trimja::ninja_clock::time_point operator()(file_clock::time_point t);
};

/**
 * @struct clock_time_conversion
 * @brief Specialization for converting between file_clock and
 * trimja::ninja_clock.
 */
template <>
struct clock_time_conversion<file_clock, trimja::ninja_clock> {
  /**
   * @brief Converts a ninja_clock::time_point to a file_clock::time_point.
   *
   * @param t The ninja_clock::time_point to convert.
   * @return The corresponding file_clock::time_point.
   */
  file_clock::time_point operator()(trimja::ninja_clock::time_point t);
};

}  // namespace chrono
}  // namespace std
#endif
#endif  // TRIMJA_NINJA_CLOCK
