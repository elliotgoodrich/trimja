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

#include "basicscope.h"

#include <utility>

namespace trimja {

BasicScope::BasicScope() = default;

BasicScope::BasicScope(BasicScope&&) noexcept = default;

BasicScope::BasicScope(const BasicScope& other) {
  for (const auto& [name, value] : other.m_variables) {
    m_variables.emplace(name.view(), value);
  }
}

BasicScope& BasicScope::operator=(BasicScope&&) noexcept = default;

BasicScope& BasicScope::operator=(const BasicScope& rhs) {
  BasicScope tmp{rhs};
  using std::swap;
  swap(m_variables, tmp.m_variables);
  return *this;
}

void BasicScope::set(std::string_view key, std::string&& value) {
  m_variables.insert_or_assign(key, std::move(value));
}

bool BasicScope::appendValue(std::string& output, std::string_view name) const {
  const auto it = m_variables.find(name);
  if (it == m_variables.end()) {
    return false;
  } else {
    output += it->second;
    return true;
  }
}

}  // namespace trimja
