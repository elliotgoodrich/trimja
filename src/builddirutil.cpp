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
#include "cpuprofiler.h"
#include "manifestparser.h"

#include <fstream>
#include <sstream>
#include <variant>

namespace trimja {

namespace {

struct BuildDirContext {
  BasicScope fileScope;

  BuildDirContext() = default;

  template <typename RANGE>
  static void consume(RANGE&& range) {
    for ([[maybe_unused]] auto&& _ : range) {
    }
  }

  void parse(const std::filesystem::path& ninjaFile,
             const std::string& ninjaFileContents) {
    for (auto&& part : ManifestReader(ninjaFile, ninjaFileContents)) {
      std::visit(*this, part);
    }
  }

  void operator()(PoolReader& r) const { consume(r.readVariables()); }

  void operator()(BuildReader& r) const {
    consume(r.readOut());
    consume(r.readImplicitOut());
    [[maybe_unused]] const std::string_view ruleName = r.readName();
    consume(r.readIn());
    consume(r.readImplicitIn());
    consume(r.readOrderOnlyDeps());
    consume(r.readValidations());
    consume(r.readVariables());
  }

  void operator()(RuleReader& r) const { consume(r.readVariables()); }

  void operator()(DefaultReader& r) const { consume(r.readPaths()); }

  void operator()(const VariableReader& r) {
    evaluate(fileScope.resetValue(r.name()), r.value(), fileScope);
  }

  void operator()(const IncludeReader& r) {
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

  void operator()(const SubninjaReader&) const {
    // subninja introduces a new scope so we can never modify the top-level
    // `builddir` variable
  }
};

}  // namespace

std::filesystem::path BuildDirUtil::builddir(
    const std::filesystem::path& ninjaFile,
    const std::string& ninjaFileContents) {
  BuildDirContext ctx;
  {
    const Timer t = CPUProfiler::start(".ninja parse");
    ctx.parse(ninjaFile, ninjaFileContents);
  }
  std::string builddir;
  ctx.fileScope.appendValue(builddir, "builddir");
  return std::filesystem::path(ninjaFile).remove_filename() / builddir;
}

}  // namespace trimja
