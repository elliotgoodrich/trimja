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

#include "builddirutil.h"

#include "basicscope.h"
#include "manifestparser.h"

#include <fstream>
#include <sstream>
#include <variant>

namespace trimja {

namespace {

struct BuildDirContext {
  BasicScope fileScope;

  BuildDirContext() = default;

  static void consume(PathRangeReader&& range) {
    for ([[maybe_unused]] const EvalString& r : range) {
    }
  }

  static void consume(LetRangeReader&& range) {
    for (VariableReader&& r : range) {
      [[maybe_unused]] const std::string_view name = r.name();
      [[maybe_unused]] const EvalString& value = r.value();
    }
  }

  void parse(const std::filesystem::path& ninjaFile,
             const std::string& ninjaFileContents) {
    for (auto&& part : ManifestReader(ninjaFile, ninjaFileContents)) {
      std::visit(*this, part);
    }
  }

  void operator()(PoolReader& r) const {
    [[maybe_unused]] const std::string_view name = r.name();
    consume(r.variables());
  }

  void operator()(BuildReader& r) const {
    consume(r.out());
    consume(r.implicitOut());
    [[maybe_unused]] const std::string_view ruleName = r.name();
    consume(r.in());
    consume(r.implicitIn());
    consume(r.orderOnlyDeps());
    consume(r.validations());
    consume(r.variables());
  }

  void operator()(RuleReader& r) const {
    [[maybe_unused]] const std::string_view name = r.name();
    consume(r.variables());
  }

  void operator()(DefaultReader& r) const { consume(r.paths()); }

  void operator()(VariableReader& r) {
    const std::string_view name = r.name();
    evaluate(fileScope.resetValue(name), r.value(), fileScope);
  }

  void operator()(IncludeReader& r) {
    const std::filesystem::path file = [&] {
      const EvalString& pathEval = r.path();
      std::string path;
      evaluate(path, pathEval, fileScope);
      return std::filesystem::path(r.parent()).remove_filename() / path;
    }();

    if (!std::filesystem::exists(file)) {
      std::string msg;
      msg += "Unable to find ";
      msg += file.string();
      msg += "!";
      throw std::runtime_error(msg);
    }
    std::stringstream ninjaCopy;
    std::ifstream ninja(file);
    ninjaCopy << ninja.rdbuf();
    parse(file, ninjaCopy.str());
  }

  void operator()(SubninjaReader& r) const {
    // subninja introduces a new scope so we can never modify the top-level
    // `builddir` variable
    [[maybe_unused]] const EvalString& p = r.path();
  }
};

}  // namespace

std::filesystem::path BuildDirUtil::builddir(
    const std::filesystem::path& ninjaFile,
    const std::string& ninjaFileContents) {
  BuildDirContext ctx;
  ctx.parse(ninjaFile, ninjaFileContents);
  std::string builddir;
  ctx.fileScope.appendValue(builddir, "builddir");
  return std::filesystem::path(ninjaFile).remove_filename() / builddir;
}

}  // namespace trimja
