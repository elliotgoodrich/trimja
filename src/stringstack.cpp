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

#include "stringstack.h"

#include <cassert>

namespace trimja {

StringStack::StringStack() : m_stack{}, m_realSize{0} {}

std::string& StringStack::operator[](std::size_t index) noexcept {
  assert(index < m_realSize);
  return m_stack[index];
}

const std::string& StringStack::operator[](std::size_t index) const noexcept {
  assert(index < m_realSize);
  return m_stack[index];
}

std::string& StringStack::emplace_back() {
  if (m_realSize < m_stack.size()) {
    std::string& top = m_stack[m_realSize++];
    top.clear();
    return top;
  } else {
    std::string& top = m_stack.emplace_back();
    // Choose something reasonable for our use case where we are most
    // likely storing file names
    top.reserve(1024);
    ++m_realSize;
    return top;
  }
}

void StringStack::pop() noexcept {
  assert(m_realSize > 0);
  --m_realSize;
}

void StringStack::clear() noexcept {
  m_realSize = 0;
}

StringStack::iterator StringStack::begin() {
  return m_stack.begin();
}

StringStack::iterator StringStack::end() {
  return m_stack.begin() +
         m_realSize;  // NOLINT(bugprone-narrowing-conversions)
}

StringStack::const_iterator StringStack::begin() const {
  return m_stack.cbegin();
}

StringStack::const_iterator StringStack::cbegin() const {
  return m_stack.cbegin();
}

StringStack::const_iterator StringStack::end() const {
  return cend();
}

StringStack::const_iterator StringStack::cend() const {
  return m_stack.cbegin() +
         m_realSize;  // NOLINT(bugprone-narrowing-conversions)
}

const std::string* StringStack::data() const {
  return m_stack.data();
}

std::size_t StringStack::size() const {
  return m_realSize;
}

bool StringStack::empty() const {
  return m_realSize == 0;
}

}  // namespace trimja
