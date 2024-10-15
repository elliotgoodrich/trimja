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

#include "evalstring.h"

namespace trimja {

Rule::Rule(std::string_view name) : m_name(name), m_bindings() {}

std::size_t Rule::getLookupIndex(std::string_view varName) {
  return std::find(std::begin(reserved), std::end(reserved), varName) -
         std::begin(reserved);
}

bool Rule::add(std::string_view varName, EvalString value) {
  const auto it = std::find_if(
      m_bindings.begin(), m_bindings.end(),
      [varName](const std::pair<const std::string_view*, EvalString>& binding) {
        return *binding.first == varName;
      });
  if (it != m_bindings.end()) {
    // If we already have this variable then it has already been verified as
    // a reserved name so no need to check against `reserved`.
    it->second = std::move(value);
  } else {
    // Otherwise we need to make sure it's actually supported
    const std::size_t lookupIndex = getLookupIndex(varName);
    if (lookupIndex == std::size(reserved)) {
      return false;
    }

    m_bindings.emplace_back(&reserved[lookupIndex], std::move(value));
  }
  return true;
}

const EvalString* Rule::lookupVar(std::string_view varName) const {
  const auto it = std::find_if(
      m_bindings.cbegin(), m_bindings.cend(),
      [varName](const std::pair<const std::string_view*, EvalString>& binding) {
        return *binding.first == varName;
      });
  return it != m_bindings.cend() ? &it->second : nullptr;
}

std::string_view Rule::name() const {
  return m_name;
}

}  // namespace trimja