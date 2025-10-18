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

#include "nestedscope.h"

#include "basicscope.h"

#include <cassert>

namespace trimja {

// In this component we lazily create our BasicScope values only when we first
// write to the scope.  This optimizes for subninja files with no additional
// variables and keeps the `appendValue` loop as short as possible.
//
// We also avoid popping elements from our `vector` so we can reuse the capacity
// from our `BasicScope` elements for the next time a new scope is added.

NestedScope::NestedScope()
    : m_scopes(1), m_scopeCount{m_scopes.size()}, m_currentIndex{0} {
  m_scopes[m_currentIndex].second = m_scopeCount;
}

std::size_t NestedScope::size() const {
  return m_scopeCount;
}

void NestedScope::push() {
  ++m_scopeCount;
}

[[nodiscard]] BasicScope NestedScope::pop() {
  assert(m_scopeCount > 1);
  BasicScope overwritten;
  auto& [topScope, scopeIndex] = m_scopes[m_currentIndex];
  if (scopeIndex == m_scopeCount) {
    using std::swap;
    swap(overwritten, topScope);
    --m_currentIndex;
  }
  --m_scopeCount;
  return overwritten;
}

void NestedScope::set(std::string_view key, std::string&& value) {
  if (m_scopes[m_currentIndex].second != m_scopeCount) {
    ++m_currentIndex;
    if (m_currentIndex == m_scopes.size()) {
      m_scopes.emplace_back();
    }
    m_scopes[m_currentIndex].second = m_scopeCount;
  }

  m_scopes[m_currentIndex].first.set(key, std::move(value));
}

bool NestedScope::appendValue(std::string& output,
                              std::string_view name) const {
  std::size_t i = m_currentIndex;
  while (true) {
    if (m_scopes[i].first.appendValue(output, name)) {
      return true;
    }
    if (i == 0) {
      return false;
    }
    --i;
  }
}

}  // namespace trimja
