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

#ifndef TRIMJA_TRIMUTIL
#define TRIMJA_TRIMUTIL

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>

namespace trimja {

namespace detail {
class Imp;
}  // namespace detail

/**
 * @class TrimUtil
 * @brief Utility to trim a Ninja build file based on a list of affected files.
 */
class TrimUtil {
  std::unique_ptr<detail::Imp> m_imp;

 public:
  /**
   * @brief Default constructor for TrimUtil.
   */
  TrimUtil();

  /**
   * @brief Destructor for TrimUtil.
   */
  ~TrimUtil();

  /**
   * @brief Trims the given Ninja build file based on the affected files.
   *
   * @param output The output stream to write the trimmed Ninja file to.
   * @param ninjaFile The path to the original Ninja build file.
   * @param ninjaFileContents The contents of the original Ninja build file.
   * @param affected The input stream containing the list of affected files.
   * @param explain If true, prints to stderr why each build command was kept.
   */
  void trim(std::ostream& output,
            const std::filesystem::path& ninjaFile,
            const std::string& ninjaFileContents,
            std::istream& affected,
            bool explain);
};

}  // namespace trimja

#endif  // TRIMJA_TRIMUTIL
