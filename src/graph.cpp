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
  std::size_t hash = 14695981039346656037ULL;
  for (const char ch : v) {
#ifdef _WIN32
    hash ^= ch == '\\' ? '/' : ch;
#else
    hash ^= ch;
#endif
    hash *= 1099511628211ULL;
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

Graph::Node Graph::addPath(std::string& path) {
  const std::size_t nextIndex = m_inputToOutput.size();
  CanonicalizePath(&path);
  const auto [it, inserted] = m_pathToNode.try_emplace(path, nextIndex, this);
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

Graph::Node Graph::addNormalizedPath(std::string_view path) {
  const std::size_t nextIndex = m_inputToOutput.size();
#ifndef NDEBUG
  std::string copy{path};
  CanonicalizePath(&copy);
  assert(copy == path);
#endif
  const auto [it, inserted] = m_pathToNode.try_emplace(path, nextIndex, this);
  if (inserted) {
    m_inputToOutput.emplace_back();
    m_outputToInput.emplace_back();
    m_path.emplace_back(it->first);
  }
  return it->second;
}

std::optional<Graph::Node> Graph::findPath(std::string& path) const {
  CanonicalizePath(&path);
  const auto it = m_pathToNode.find(path);
  if (it == m_pathToNode.end()) {
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

std::optional<Graph::Node> Graph::findNormalizedPath(
    std::string_view path) const {
  const auto it = m_pathToNode.find(path);
  if (it == m_pathToNode.end()) {
    return std::nullopt;
  } else {
    return it->second;
  }
}

Graph::Node Graph::addDefault() {
  assert(!m_defaultNode.has_value());
  const Node node{m_inputToOutput.size(), this};
  m_inputToOutput.emplace_back();
  m_outputToInput.emplace_back();
  m_path.emplace_back("default");
  m_defaultNode = node;
  return node;
}

void Graph::addEdge(Graph::Node in, Graph::Node out) {
  assert(in != out);
  m_inputToOutput[in].push_back(out);
  m_outputToInput[out].push_back(in);
}

void Graph::addOneWayEdge(Graph::Node in, Graph::Node out) {
  assert(in != out);
  m_inputToOutput[in].push_back(out);
}

bool Graph::isDefault(Graph::Node node) const {
  return node == m_defaultNode;
}

std::optional<Graph::Node> Graph::getDefault() const {
  return m_defaultNode;
}

std::string_view Graph::path(Graph::Node node) const {
  return m_path[node];
}

const gch::small_vector<Graph::Node>& Graph::out(Graph::Node node) const {
  return m_inputToOutput[node];
}

const gch::small_vector<Graph::Node>& Graph::in(Graph::Node node) const {
  return m_outputToInput[node];
}

std::size_t Graph::size() const {
  return m_inputToOutput.size();
}

IndexIntoRange<Graph::Node> Graph::nodes() const {
  return IndexIntoRange<Graph::Node>{Node{0, this},
                                     Node{m_inputToOutput.size(), this}};
}

}  // namespace trimja
