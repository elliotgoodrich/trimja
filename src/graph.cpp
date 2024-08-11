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

#include "graph.h"

#include <cassert>

namespace trimja {

Graph::Graph() = default;

std::size_t Graph::addPath(const std::string& path) {
  const std::size_t nextIndex = size();
  const auto [it, inserted] = m_pathToIndex.emplace(path, nextIndex);
  if (inserted) {
    m_inputToOutput.emplace_back();
    m_outputToInput.emplace_back();
  }
  return it->second;
}

void Graph::addEdge(std::size_t index, std::size_t dependsOn) {
  assert(index != dependsOn);
  m_inputToOutput[index].insert(dependsOn);
  m_outputToInput[dependsOn].insert(index);
}

const std::set<std::size_t>& Graph::out(std::size_t pathIndex) const {
  return m_inputToOutput[pathIndex];
}

const std::set<std::size_t>& Graph::in(std::size_t pathIndex) const {
  return m_outputToInput[pathIndex];
}

std::size_t Graph::getPath(const std::string& path) const {
  return m_pathToIndex.find(path)->second;
}

std::size_t Graph::size() const {
  return m_pathToIndex.size();
}

}  // namespace trimja
