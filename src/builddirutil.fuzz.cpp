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

#include "manifestparser.h"
#include "manifestwriter.h"

#include <sstream>
#include <string>

#include <stdint.h>  // NOLINT(modernize-deprecated-headers)

namespace {

using namespace trimja;

class Context {
  ManifestWriter m_writer;

  template <typename WRITER, typename INPUT_RANGE>
  void writeVariables(WRITER& writer, INPUT_RANGE&& variables) {
    for (const auto& [name, value] : variables) {
      writer.variable(name, value);
    }
  }

 public:
  explicit Context(std::ostream& out) : m_writer{out} {}

  void parse(const std::string& ninjaFileContents) {
    for (auto&& part : ManifestReader{"", ninjaFileContents}) {
      std::visit(*this, part);
    }
  }

  void operator()(PoolReader& r) {
    auto writer = m_writer.pool(r.name());
    writeVariables(writer, r.readVariables());
  }

  void operator()(BuildReader& r) {
    auto writer = m_writer.build()
                      .out(r.readOut())
                      .implicitOut(r.readImplicitOut())
                      .name(r.readName())
                      .in(r.readIn())
                      .orderOnlyDeps(r.readOrderOnlyDeps())
                      .validations(r.readValidations());
    writeVariables(writer, r.readVariables());
  }

  void operator()(RuleReader& r) {
    auto writer = m_writer.rule(r.name());
    writeVariables(writer, r.readVariables());
  }

  void operator()(DefaultReader& r) { m_writer.default_(r.readPaths()); }

  void operator()(const VariableReader& r) {
    m_writer.variable(r.name(), r.value());
  }

  void operator()(const IncludeReader& r) { m_writer.include(r.path()); }

  void operator()(const SubninjaReader& r) { m_writer.subninja(r.path()); }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  using namespace trimja;
  const std::string input{reinterpret_cast<const char*>(data), size};
  std::ostringstream out;
  try {
    Context ctx{out};
    ctx.parse(input);
    std::string first = out.str();
    out.str("");
    ctx.parse(input);
    assert(first == out.str());

  } catch (const std::exception&) {
  }
  return 0;
}
