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

#ifndef TRIMJA_DEPSWRITER
#define TRIMJA_DEPSWRITER

#include "ninja_clock.h"

#include <cstdint>
#include <iosfwd>
#include <span>
#include <string_view>

namespace trimja {

/**
 * @struct DepsWriter
 * @brief A class to write dependencies to an output stream.
 */
struct DepsWriter {
  /**
   * @brief Constructs a DepsWriter with the given output stream.  Note that
   * `out` must outlive the DepsWriter and should be open in binary mode.
   *
   * @param out The output stream to write dependencies to.
   */
  explicit DepsWriter(std::ostream& out);

  /**
   * @brief Records a path and returns its associated node ID.
   *
   * @param path The path to record.
   * @param nodeID The associated node ID.  Note that if this is not provided
   * then an incrementing node ID will be used.
   * @return The node ID associated with the recorded path.
   */
  std::int32_t recordPath(std::string_view path);
  std::int32_t recordPath(std::string_view path, std::int32_t nodeId);

  /**
   * @brief Records dependencies for a given output node.
   *
   * @param out The node ID of the output.
   * @param mtime The modification time of the output.
   * @param dependencies A span of node IDs representing the dependencies.
   */
  void recordDependencies(std::int32_t out,
                          ninja_clock::time_point mtime,
                          std::span<const std::int32_t> dependencies);

 private:
  std::ostream* m_out;
  std::int32_t m_nextNode;
};

}  // namespace trimja

#endif  // TRIMJA_DEPSWRITER
