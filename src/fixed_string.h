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

// `fixed_string` is a non-null terminated string that cannot have its length
// changed after construction.  It is useful as the key to an associative
// container since these are immutable after construction.  It has no
// small-string optimization (yet).
//
// Before we have heterogenous lookup in associative containers (e.g.
// `is_transparent` on hash and equality) we can use the `make_temp` static
// method to create a `const fixed_string&` that can be used to lookup
// `std::string_view` values from containers.
class fixed_string {
  std::string_view m_data;

 public:
  struct copy_from_string_view_t {
    std::string_view data;
  };

  // Create a `const fixed_string&` from the specified `str`.  The lifetime of
  // the returned reference is the same as the lifetime of `str`.
  static const fixed_string& make_temp(const std::string_view& str) noexcept;

  // Create an object that can construct a `fixed_string` from the specified
  // `str`.
  static copy_from_string_view_t create(const std::string_view& str);

  // Create a `fixed_string` from the specified `str`. We do not have a
  // constructor from `std::string_view` in case users forget to call
  // `make_temp` when looking up values and we construct a temporary
  // `fixed_string`
  fixed_string(const copy_from_string_view_t& str);

  ~fixed_string();

  fixed_string(const fixed_string&) = delete;
  fixed_string& operator=(const fixed_string&) = delete;

  operator std::string_view() const;
  std::string_view view() const;
};

bool operator==(const fixed_string& lhs, const fixed_string& rhs);
bool operator!=(const fixed_string& lhs, const fixed_string& rhs);

}  // namespace trimja

namespace std {

template <>
struct hash<trimja::fixed_string> {
  size_t operator()(const trimja::fixed_string& str) const {
    return hash<std::string_view>{}(str.view());
  }
};

}  // namespace std

#endif  // TRIMJA_FIXED_STRING
