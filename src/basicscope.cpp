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

namespace trimja {

BasicScope::BasicScope() = default;

std::string_view BasicScope::set(std::string_view key, std::string&& value) {
  // By design to avoid accidental copies, `fixed_string` does not have a copy
  // constructor so we cannot use `operator[]`.  Instead we can use `emplace` to
  // achieve basically the same performance.
  return m_variables.emplace(fixed_string::create(key), "").first->second =
             std::move(value);
}

bool BasicScope::appendValue(std::string& output, std::string_view name) const {
  const auto it = m_variables.find(fixed_string::make_temp(name));
  if (it == m_variables.end()) {
    return false;
  } else {
    output += it->second;
    return true;
  }
}

}  // namespace trimja
