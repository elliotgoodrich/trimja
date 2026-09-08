// MIT License
//
// Copyright (c) 2026 Elliot Goodrich
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

#ifndef TRIMJA_FILEUTIL
#define TRIMJA_FILEUTIL

#include <filesystem>
#include <fstream>
#include <ios>

namespace trimja {

/**
 * @class FileUtil
 * @brief Utility functions for reading files from disk.
 */
struct FileUtil {
  /**
   * @brief Open a file for reading, throwing if it cannot be read.
   *
   * Rather than checking `std::filesystem::exists` and then opening, this opens
   * the file exactly once - closing the TOCTOU gap - and treats a missing file,
   * a file we cannot read (e.g. no permission), and a directory all as errors.
   * This matches ninja, which fails on any unsuccessful read.  An empty but
   * readable file opens successfully.
   *
   * @param file The path to open.
   * @param mode The open mode, defaulting to reading.
   * @return The opened input stream.
   * @throws std::runtime_error if the file cannot be opened or read.
   */
  static std::ifstream openFile(
      const std::filesystem::path& file,
      std::ios_base::openmode mode = std::ios_base::in);
};

}  // namespace trimja

#endif  // TRIMJA_FILEUTIL
