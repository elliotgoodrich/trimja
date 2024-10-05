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

#ifndef TRIMJA_BASICSCOPE
#define TRIMJA_BASICSCOPE

#include "fixed_string.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace trimja {

// `BasicScope` holds a set of key-value pairs that represent ninja variables.
class BasicScope {
  std::unordered_map<fixed_string, std::string> m_variables;

 public:
  // Create a `BasicScope` with no values.
  BasicScope();

  // Set the value of the specified `key` to the specified `value` and return a
  // `std::string_view` to to the inserted value.  Note that `value` will be
  // moved from unconditionally.
  std::string_view set(std::string_view key, std::string&& value);

  // Insert an empty value for the specified `key`, or clear the existing
  // `value` if there is one and return a reference to this empty value in both
  // cases.
  std::string& clearValue(std::string_view key);

  // If there is a key with the specified `name`, then append its value to the
  // specified `output` and return true; otherwise do nothing and return false.
  bool appendValue(std::string& output, std::string_view name) const;
};

}  // namespace trimja

#endif