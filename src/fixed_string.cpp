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

#include "fixed_string.h"

#include <algorithm>

namespace trimja {

const fixed_string& fixed_string::make_temp(
    const std::string_view& str) noexcept {
  // This is only defined as `fixed_string` is standard layout (meaning all its
  // members must be standard layout too) and so we can `reinterpret_cast`
  // ourselves to the first member variable.
  static_assert(std::is_standard_layout_v<fixed_string>);
  return reinterpret_cast<const fixed_string&>(str);
}

fixed_string::copy_from_string_view_t fixed_string::create(
    const std::string_view& str) {
  return copy_from_string_view_t{str};
}

fixed_string::fixed_string(const copy_from_string_view_t& str) : m_data{} {
  const std::size_t length = str.data.size();
  char* data = new char[length];
  std::copy_n(str.data.data(), length, data);
  m_data = std::string_view{data, length};
}

fixed_string::~fixed_string() {
  delete[] m_data.data();
}

fixed_string::fixed_string(fixed_string&& other) noexcept
    : m_data(std::exchange(other.m_data, std::string_view{nullptr, 0})) {}

fixed_string& fixed_string::operator=(fixed_string&& rhs) noexcept {
  fixed_string tmp{std::move(rhs)};
  using std::swap;
  swap(m_data, tmp.m_data);
  return *this;
}

fixed_string::operator std::string_view() const {
  return m_data;
}

std::string_view fixed_string::view() const {
  return m_data;
}

bool operator==(const fixed_string& lhs, const fixed_string& rhs) {
  return lhs.view() == rhs.view();
}

bool operator!=(const fixed_string& lhs, const fixed_string& rhs) {
  return lhs.view() != rhs.view();
}

std::size_t hash_value(const fixed_string& str) {
  return std::hash<fixed_string>{}(str);
}

}  // namespace trimja

namespace std {

size_t hash<trimja::fixed_string>::operator()(
    const trimja::fixed_string& str) const {
  return hash<std::string_view>{}(str.view());
}

}  // namespace std
