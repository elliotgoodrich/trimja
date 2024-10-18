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

namespace {

std::string_view cloneStringView(std::string_view str) {
  const std::size_t length = str.size();
  char* data = new char[length];
  std::copy_n(str.data(), length, data);
  return std::string_view{data, length};
}

}  // namespace

fixed_string::fixed_string() : m_data{nullptr, 0} {}

fixed_string::fixed_string(std::string_view str)
    : m_data{cloneStringView(str)} {}

fixed_string::~fixed_string() {
  delete[] m_data.data();
}

fixed_string::fixed_string(fixed_string&& other) noexcept
    : m_data{std::exchange(other.m_data, std::string_view{nullptr, 0})} {}

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

size_t hash<trimja::fixed_string>::operator()(const string_view& str) const {
  return hash<string_view>{}(str);
}

size_t hash<trimja::fixed_string>::operator()(
    const trimja::fixed_string& str) const {
  return hash<string_view>{}(str.view());
}

bool equal_to<trimja::fixed_string>::operator()(
    const string_view& lhs,
    const trimja::fixed_string& rhs) const {
  return lhs == rhs.view();
}

bool equal_to<trimja::fixed_string>::operator()(
    const trimja::fixed_string& lhs,
    const trimja::fixed_string& rhs) const {
  return lhs == rhs;
}

}  // namespace std
