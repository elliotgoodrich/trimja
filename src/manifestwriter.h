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

#ifndef TRIMJA_MANIFESTWRITER
#define TRIMJA_MANIFESTWRITER

#include "evalstring.h"

#include <iosfwd>
#include <string_view>

namespace trimja {

/**
 * Implementation note: string separators are constructed as `std::string_view`
 * in the header to avoid needing to include <ostream> over <iosfwd>.
 */

class NestedVariableWriter {
  std::ostream& m_out;

 public:
  explicit NestedVariableWriter(std::ostream& out);

  void variable(std::string_view name, const EvalString& value);
};

class BuildValidationWriter {
 protected:
  std::ostream& m_out;

 public:
  explicit BuildValidationWriter(std::ostream& out);

  template <typename INPUT_RANGE>
  NestedVariableWriter validations(INPUT_RANGE&& paths);
};

class BuildOrderOnlyDepsWriter : protected BuildValidationWriter {
 public:
  using BuildValidationWriter::BuildValidationWriter;

  template <typename INPUT_RANGE>
  BuildValidationWriter orderOnlyDeps(INPUT_RANGE&& paths);
  using BuildValidationWriter::validations;
};

class BuildImplicitInWriter : protected BuildOrderOnlyDepsWriter {
 public:
  using BuildOrderOnlyDepsWriter::BuildOrderOnlyDepsWriter;

  template <typename INPUT_RANGE>
  BuildOrderOnlyDepsWriter implicitIn(INPUT_RANGE&& paths);
  using BuildOrderOnlyDepsWriter::orderOnlyDeps;
  using BuildOrderOnlyDepsWriter::validations;
};

class BuildInWriter : protected BuildImplicitInWriter {
 public:
  using BuildImplicitInWriter::BuildImplicitInWriter;

  template <typename INPUT_RANGE>
  BuildImplicitInWriter in(INPUT_RANGE&& paths);
  using BuildImplicitInWriter::implicitIn;
  using BuildImplicitInWriter::orderOnlyDeps;
  using BuildImplicitInWriter::validations;
};

class BuildNameWriter {
 protected:
  std::ostream& m_out;

 public:
  explicit BuildNameWriter(std::ostream& out);

  BuildInWriter name(std::string_view name);
};

class BuildImplicitOutWriter : protected BuildNameWriter {
 public:
  using BuildNameWriter::BuildNameWriter;

  template <typename INPUT_RANGE>
  BuildNameWriter implicitOut(INPUT_RANGE&& paths);
  using BuildNameWriter::name;
};

class BuildOutWriter : protected BuildImplicitOutWriter {
 public:
  using BuildImplicitOutWriter::BuildImplicitOutWriter;

  template <typename INPUT_RANGE>
  BuildImplicitOutWriter out(INPUT_RANGE&& paths);
  using BuildImplicitOutWriter::implicitOut;
  using BuildImplicitOutWriter::name;
};

class ManifestWriter {
  std::ostream& m_out;

 public:
  explicit ManifestWriter(std::ostream& out);

  void variable(std::string_view name, const EvalString& value);

  NestedVariableWriter pool(std::string_view name);

  template <typename INPUT_RANGE>
  void default_(INPUT_RANGE&& paths) {
    m_out << std::string_view{"default "};
    std::string_view separator = "";
    for (const EvalString& path : paths) {
      m_out << separator << path;
      separator = " ";
    }
    m_out << std::string_view{"\n"};
  }

  BuildOutWriter build();

  NestedVariableWriter rule(std::string_view name);

  void subninja(const EvalString& path);
  void include(const EvalString& path);
};

template <typename INPUT_RANGE>
NestedVariableWriter BuildValidationWriter::validations(INPUT_RANGE&& paths) {
  std::string_view separator = " |@ ";
  for (const EvalString& path : paths) {
    m_out << separator << path;
    separator = " ";
  }
  return NestedVariableWriter{m_out};
}

template <typename INPUT_RANGE>
BuildValidationWriter BuildOrderOnlyDepsWriter::orderOnlyDeps(
    INPUT_RANGE&& paths) {
  std::string_view separator = " || ";
  for (const EvalString& path : paths) {
    m_out << separator << path;
    separator = " ";
  }
  return BuildValidationWriter{m_out};
}

template <typename INPUT_RANGE>
BuildOrderOnlyDepsWriter BuildImplicitInWriter::implicitIn(
    INPUT_RANGE&& paths) {
  std::string_view separator = " | ";
  for (const EvalString& path : paths) {
    m_out << separator << path;
    separator = " ";
  }
  return BuildOrderOnlyDepsWriter{m_out};
}

template <typename INPUT_RANGE>
BuildImplicitInWriter BuildInWriter::in(INPUT_RANGE&& paths) {
  for (const EvalString& path : paths) {
    m_out << std::string_view{" "} << path;
  }
  return BuildImplicitInWriter{m_out};
}

template <typename INPUT_RANGE>
BuildNameWriter BuildImplicitOutWriter::implicitOut(INPUT_RANGE&& paths) {
  std::string_view separator = " | ";
  for (const EvalString& path : paths) {
    m_out << separator << path;
    separator = " ";
  }
  return BuildNameWriter{m_out};
}

template <typename INPUT_RANGE>
BuildImplicitOutWriter BuildOutWriter::out(INPUT_RANGE&& paths) {
  for (const EvalString& path : paths) {
    m_out << std::string_view{" "} << path;
  }
  return BuildImplicitOutWriter{m_out};
}

}  // namespace trimja

#endif
