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

namespace detail {

class BuildDirContext {
 public:
  BasicScope fileScope;

  BuildDirContext() = default;

  void parse(const std::filesystem::path& ninjaFile,
             std::string_view ninjaFileContents) {
    for (auto&& part : ManifestReader{ninjaFile, ninjaFileContents}) {
      std::visit(*this, part);
    }
  }

  void operator()(auto& r) const { r.skip(); }

  void operator()(VariableReader& r) {
    std::string value;
    evaluate(value, r.value(), fileScope);
    fileScope.set(r.name(), std::move(value));
  }

  void operator()(IncludeReader& r) {
    // We handle include, but never subninja as it introduces a new scope so we
    // can never modify the top-level `builddir` variable

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
    const std::ifstream ninja{file};
    ninjaCopy << ninja.rdbuf();
    ninjaCopy << '\0';  // ensure our `string_view` is null-terminated
    parse(file, ninjaCopy.view());
  }
};

}  // namespace detail

BuildDirUtil::BuildDirUtil() : m_imp{nullptr} {}

BuildDirUtil::~BuildDirUtil() = default;

std::filesystem::path BuildDirUtil::builddir(
    const std::filesystem::path& ninjaFile,
    const std::string& ninjaFileContents) {
  // Keep our state inside `m_imp` so that we defer cleanup until the destructor
  // of `BuildDirUtil`. This allows the calling code to skip all destructors
  // when calling `std::_Exit`.
  m_imp = std::make_unique<detail::BuildDirContext>();
  {
    const Timer t = CPUProfiler::start(".ninja parse");
    m_imp->parse(ninjaFile, ninjaFileContents);
  }
  std::string builddir;
  m_imp->fileScope.appendValue(builddir, "builddir");
  return std::filesystem::path(ninjaFile).remove_filename() / builddir;
}

}  // namespace trimja
