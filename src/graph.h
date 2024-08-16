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

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace trimja {

class Graph {
  // A look up from path to vertex index.
  // TODO: Change the key to `std::string_view`
  std::unordered_map<std::string, std::size_t> m_pathToIndex;

  // An adjacency list of input -> output
  std::vector<std::set<std::size_t>> m_inputToOutput;

  // An adjacency list of output -> Input
  std::vector<std::set<std::size_t>> m_outputToInput;

  // names of paths
  std::vector<std::string> m_path;

  std::size_t m_defaultIndex = -1;

 public:
  Graph();

  std::size_t addPath(const std::string& path);

  std::size_t addDefault();

  void addEdge(std::size_t in, std::size_t out);

  bool isDefault(std::size_t pathIndex) const;

  std::string_view path(std::size_t pathIndex) const;
  const std::set<std::size_t>& out(std::size_t pathIndex) const;
  const std::set<std::size_t>& in(std::size_t pathIndex) const;

  std::size_t getPath(const std::string& path) const;

  std::size_t size() const;
};

}  // namespace trimja

#endif  // TRIMJA_TRIMUTIL
