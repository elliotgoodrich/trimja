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

#include "manifestwriter.h"

#include <ostream>

namespace trimja {

NestedVariableWriter::NestedVariableWriter(std::ostream& out) : m_out{out} {}

void NestedVariableWriter::variable(std::string_view name,
                                    const EvalString& value) {
  m_out << '\n' << name << ' = ' << value;
}

BuildValidationWriter::BuildValidationWriter(std::ostream& out) : m_out{out} {}

BuildNameWriter::BuildNameWriter(std::ostream& out) : m_out{out} {}

BuildInWriter BuildNameWriter::name(std::string_view name) {
  m_out << name << ':';
  return BuildInWriter{m_out};
}

ManifestWriter::ManifestWriter(std::ostream& out) : m_out{out} {}

void ManifestWriter::variable(std::string_view name,
                                         const EvalString& value) {
  m_out << name << " = " << value << '\n';
}

NestedVariableWriter ManifestWriter::pool(std::string_view name) {
  m_out << "pool " << name << '\n';
  return NestedVariableWriter{m_out};
}

BuildOutWriter ManifestWriter::build() {
  return BuildOutWriter{m_out};
}

NestedVariableWriter ManifestWriter::rule(std::string_view name) {
  m_out << "rule " << name << '\n';
  return NestedVariableWriter{m_out};
}

void ManifestWriter::subninja(const EvalString& path) {
  m_out << "subninja " << path << '\n';
}

void ManifestWriter::include(const EvalString& path) {
  m_out << "include " << path << '\n';
}

}  // namespace trimja
