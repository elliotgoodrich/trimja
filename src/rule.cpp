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

#include "rule.h"

#include <ninja/eval_env.h>

namespace trimja {

Rule::Rule(std::string_view name) : m_name(name), m_lookup(), m_bindings() {
  m_lookup.fill(std::numeric_limits<unsigned char>::max());
}

std::size_t Rule::getLookupIndex(std::string_view varName) {
  return std::find(reserved.begin(), reserved.end(), varName) -
         reserved.begin();
}

bool Rule::add(std::string_view varName, EvalString&& value) {
  const std::size_t lookupIndex = getLookupIndex(varName);
  if (lookupIndex == reserved.size()) {
    return false;
  }
  const std::size_t bindingIndex = m_lookup[lookupIndex];
  if (bindingIndex < m_bindings.size()) {
    m_bindings[bindingIndex] = std::move(value);
  } else {
    m_bindings.push_back(std::move(value));
    m_lookup[lookupIndex] = static_cast<unsigned char>(m_bindings.size() - 1);
  }
  return true;
}

bool Rule::add(std::string_view varName, const EvalString& value) {
  const std::size_t lookupIndex = getLookupIndex(varName);
  if (lookupIndex == reserved.size()) {
    return false;
  }
  const std::size_t bindingIndex = m_lookup[lookupIndex];
  if (bindingIndex < m_bindings.size()) {
    m_bindings[bindingIndex] = value;
  } else {
    m_bindings.push_back(value);
    m_lookup[lookupIndex] = static_cast<unsigned char>(m_bindings.size() - 1);
  }
  return true;
}

const EvalString* Rule::lookupVar(std::string_view varName) const {
  const std::size_t bindingIndex = m_lookup[getLookupIndex(varName)];
  return bindingIndex < m_bindings.size() ? &m_bindings[bindingIndex] : nullptr;
}

std::string_view Rule::name() const {
  return m_name;
}

}  // namespace trimja