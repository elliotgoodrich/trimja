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

#include "edgescope.h"

#include <ninja/util.h>

namespace trimja {

namespace detail {

void EdgeScopeBase::appendPaths(std::string& output,
                                std::span<const std::string> paths,
                                const char separator) {
  auto it = paths.begin();
  const auto end = paths.end();
  if (it == end) {
    return;
  }

  goto skipSeparator;
  for (; it != end; ++it) {
    output += separator;
  skipSeparator:
    appendEscapedString(output, *it);
  }
}

EdgeScopeBase::EdgeScopeBase(const Rule& rule,
                             std::span<const std::string> ins,
                             std::span<const std::string> outs)
    : m_ins(ins), m_outs(outs), m_local(), m_rule(rule) {}

std::string_view EdgeScopeBase::set(std::string_view key, std::string&& value) {
  return m_local.set(key, std::move(value));
}

std::string& EdgeScopeBase::resetValue(std::string_view key) {
  return m_local.resetValue(key);
}

}  // namespace detail
}  // namespace trimja
