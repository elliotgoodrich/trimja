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

#ifndef TRIMJA_GRAPH
#define TRIMJA_GRAPH

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace trimja {

class Graph {
  struct PathHash {
    using is_transparent = void;
    std::size_t operator()(const std::string& v) const;
    std::size_t operator()(std::string_view v) const;
  };

  struct PathEqual {
    using is_transparent = void;
    bool operator()(std::string_view left, std::string_view right) const;
  };

  // A look up from path to vertex index.
  std::unordered_map<std::string, std::size_t, PathHash, PathEqual>
      m_pathToIndex;

  // An adjacency list of input -> output
  std::vector<std::set<std::size_t>> m_inputToOutput;

  // An adjacency list of output -> Input
  std::vector<std::set<std::size_t>> m_outputToInput;

  // Names of paths (this points to the keys in `m_pathToIndex`, which is always
  // valid since `std::unordered_map` has pointer stability.
  std::vector<std::string_view> m_path;

  std::size_t m_defaultIndex = std::numeric_limits<std::size_t>::max();

 public:
  Graph();

  Graph(Graph&&) = default;
  Graph(const Graph&) = delete;
  Graph& operator=(Graph&&) = default;
  Graph& operator=(const Graph&) = delete;

  // Add the specified `path` to the graph if it isn't already and then return
  // the corresponding index to that path. `path` will be modified to be
  // normalized.  Note that on windows, backslashes and forward slashes are
  // accepted as valid path separators.  If `addPath` is called multiple times
  // with a `path` that differs only by the slash type, then `path` will be
  // updated to have the slashes of the first `path` called.
  std::size_t addPath(std::string& path);

  std::size_t addNormalizedPath(std::string_view path);

  std::optional<std::size_t> findPath(std::string& path) const;
  std::optional<std::size_t> findNormalizedPath(std::string_view path) const;

  std::size_t addDefault();

  void addEdge(std::size_t in, std::size_t out);

  bool isDefault(std::size_t pathIndex) const;
  std::size_t defaultIndex() const;

  std::string_view path(std::size_t pathIndex) const;
  const std::set<std::size_t>& out(std::size_t pathIndex) const;
  const std::set<std::size_t>& in(std::size_t pathIndex) const;

  std::size_t getPath(std::string_view path) const;

  std::size_t size() const;
};

}  // namespace trimja

#endif  // TRIMJA_GRAPH