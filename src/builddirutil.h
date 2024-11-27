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

#ifndef TRIMJA_BUILDDIRUTIL
#define TRIMJA_BUILDDIRUTIL

#include <filesystem>
#include <string>

namespace trimja {

namespace detail {
class BuildDirContext;
}  // namespace detail

/**
 * @class BuildDirUtil
 * @brief Utility functions for finding the build directory for a Ninja file.
 */
class BuildDirUtil {
  std::unique_ptr<detail::BuildDirContext> m_imp;

 public:
  /**
   * @brief Default constructor for BuildDirUtil.
   */
  BuildDirUtil();

  /**
   * @brief Destructor for BuildDirUtil.
   */
  ~BuildDirUtil();

  /**
   * @brief Determines the build directory from the given Ninja file and its
   * contents.
   * @param ninjaFile The path to the Ninja file.
   * @param ninjaFileContents The contents of the Ninja file.
   * @return The path to the build directory.
   */
  std::filesystem::path builddir(const std::filesystem::path& ninjaFile,
                                 const std::string& ninjaFileContents);
};

}  // namespace trimja

#endif  // TRIMJA_BUILDDIRUTIL
