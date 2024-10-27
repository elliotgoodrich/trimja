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

#include <ninja/util.h>

#include <cassert>

namespace trimja {

std::size_t Graph::PathHash::operator()(const fixed_string& v) const {
  return (*this)(v.view());
}

std::size_t Graph::PathHash::operator()(std::string_view v) const {
  // FNV-1a hash algorithm but on Windows we swap all backslashes with forward
  // slashes
  std::size_t hash = 14695981039346656037ull;
  for (const char ch : v) {
#ifdef _WIN32
    hash ^= ch == '\\' ? '/' : ch;
#else
    hash ^= ch;
#endif
    hash *= 1099511628211ull;
  }
  return hash;
}

bool Graph::PathEqual::operator()(const fixed_string& left,
                                  const fixed_string& right) const {
  return (*this)(left.view(), right);
}

bool Graph::PathEqual::operator()(std::string_view left,
                                  const fixed_string& right) const {
#ifdef _WIN32
  return std::equal(left.begin(), left.end(), right.view().begin(),
                    right.view().end(), [](char l, char r) {
                      return (l == '\\' ? '/' : l) == (r == '\\' ? '/' : r);
                    });
#else
  return left == right.view();
#endif
}

Graph::Graph() = default;

std::size_t Graph::addPath(std::string& path) {
  const std::size_t nextIndex = m_inputToOutput.size();
  CanonicalizePath(&path);
  const auto [it, inserted] = m_pathToIndex.try_emplace(path, nextIndex);
  if (inserted) {
    m_inputToOutput.emplace_back();
    m_outputToInput.emplace_back();
    m_path.emplace_back(it->first);
  }
#ifdef _WIN32
  else {
    // On windows paths may differ so update `path` here with the canonical
    // one, which may differ by path separators
    path = it->first;
  }
#endif
  return it->second;
}

std::size_t Graph::addNormalizedPath(std::string_view path) {
  const std::size_t nextIndex = m_inputToOutput.size();
#ifndef NDEBUG
  std::string copy{path};
  CanonicalizePath(&copy);
  assert(copy == path);
#endif
  const auto [it, inserted] = m_pathToIndex.try_emplace(path, nextIndex);
  if (inserted) {
    m_inputToOutput.emplace_back();
    m_outputToInput.emplace_back();
    m_path.emplace_back(it->first);
  }
  return it->second;
}

std::optional<std::size_t> Graph::findPath(std::string& path) const {
  CanonicalizePath(&path);
  const auto it = m_pathToIndex.find(path);
  if (it == m_pathToIndex.end()) {
    return std::nullopt;
  } else {
#ifdef _WIN32
    // On windows paths may differ so update `path` here with the canonical
    // one
    path = it->first;
#endif
    assert(it->first.view() == path);
    return it->second;
  }
}

std::optional<std::size_t> Graph::findNormalizedPath(
    std::string_view path) const {
  const auto it = m_pathToIndex.find(path);
  if (it == m_pathToIndex.end()) {
    return std::nullopt;
  } else {
    return it->second;
  }
}

std::size_t Graph::addDefault() {
  assert(m_defaultIndex == std::numeric_limits<std::size_t>::max());
  const std::size_t nextIndex = m_inputToOutput.size();
  m_inputToOutput.emplace_back();
  m_outputToInput.emplace_back();
  m_path.push_back("default");
  m_defaultIndex = nextIndex;
  return nextIndex;
}

void Graph::addEdge(std::size_t in, std::size_t out) {
  assert(in != out);
  m_inputToOutput[in].push_back(out);
  m_outputToInput[out].push_back(in);
}

void Graph::addOneWayEdge(std::size_t in, std::size_t out) {
  assert(in != out);
  m_inputToOutput[in].push_back(out);
}

bool Graph::isDefault(std::size_t pathIndex) const {
  return pathIndex == m_defaultIndex;
}

std::size_t Graph::defaultIndex() const {
  return m_defaultIndex;
}

std::string_view Graph::path(std::size_t pathIndex) const {
  return m_path[pathIndex];
}

const gch::small_vector<std::size_t>& Graph::out(std::size_t pathIndex) const {
  return m_inputToOutput[pathIndex];
}

const gch::small_vector<std::size_t>& Graph::in(std::size_t pathIndex) const {
  return m_outputToInput[pathIndex];
}

std::size_t Graph::getPath(std::string_view path) const {
  return m_pathToIndex.find(path)->second;
}

std::size_t Graph::size() const {
  return m_inputToOutput.size();
}

}  // namespace trimja
