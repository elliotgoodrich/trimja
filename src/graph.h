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

#include "fixed_string.h"

#include <boost/boost_unordered.hpp>
#include <gch/small_vector.hpp>

#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trimja {

/**
 * @class Graph
 * @brief Represents a directed graph where nodes are file paths and edges
 * represent dependencies.
 */
class Graph {
  struct PathHash {
    using is_transparent = void;

    // We need `PathHash` to have some size, otherwise we hit a compilation
    // issue with `boost::unordered_flat_map`.
    void* _;
    std::size_t operator()(const fixed_string& v) const;
    std::size_t operator()(std::string_view v) const;
  };

  struct PathEqual {
    using is_transparent = void;

    // We need `PathEqual` to have some size, otherwise we hit a compilation
    // issue with `boost::unordered_flat_map`.
    void* _;
    bool operator()(const fixed_string& left, const fixed_string& right) const;
    bool operator()(std::string_view left, const fixed_string& right) const;
  };

 private:
  // A look up from path to vertex index.
  boost::unordered_flat_map<fixed_string, std::size_t, PathHash, PathEqual>
      m_pathToIndex;

  // An adjacency list of input -> output
  std::vector<gch::small_vector<std::size_t>> m_inputToOutput;

  // An adjacency list of output -> Input
  std::vector<gch::small_vector<std::size_t>> m_outputToInput;

  // Names of paths (this points to the keys in `m_pathToIndex`, which is always
  // valid since `fixed_string` has no small-string optimization and always
  // allocates on the heap.
  std::vector<std::string_view> m_path;

  std::optional<std::size_t> m_defaultIndex;

 public:
  /**
   * @brief Constructs an empty Graph.
   */
  Graph();

  Graph(Graph&&) = default;
  Graph(const Graph&) = delete;
  Graph& operator=(Graph&&) = default;
  Graph& operator=(const Graph&) = delete;

  /**
   * @brief Adds the path to the graph if it isn't already present and returns
   * the corresponding index.
   * @param path The path to be added. It will be normalized.
   * @return The index corresponding to the added path.
   */
  std::size_t addPath(std::string& path);

  /**
   * @brief Adds a normalized path to the graph if it isn't already present and
   * returns the corresponding index.
   * @param path The normalized path to be added.
   * @return The index corresponding to the added path.
   */
  std::size_t addNormalizedPath(std::string_view path);

  /**
   * @brief Finds the index of the specified path if it exists.
   * @param path The path to be found. It will be normalized.
   * @return The index of the path if found, otherwise std::nullopt.
   */
  std::optional<std::size_t> findPath(std::string& path) const;

  /**
   * @brief Finds the index of the specified normalized path if it exists.
   * @param path The normalized path to be found.
   * @return The index of the path if found, otherwise std::nullopt.
   */
  std::optional<std::size_t> findNormalizedPath(std::string_view path) const;

  /**
   * @brief Adds a default node to the graph.
   * @return The index of the default node.
   */
  std::size_t addDefault();

  /**
   * @brief Adds bidirectional edge between the specified input and output
   * nodes.
   * @param in The index of the input node.
   * @param out The index of the output node.
   */
  void addEdge(std::size_t in, std::size_t out);

  /**
   * @brief Adds an one-way edge from the specified input to output node.
   * @param in The index of the input node.
   * @param out The index of the output node.
   */
  void addOneWayEdge(std::size_t in, std::size_t out);

  /**
   * @brief Checks if the specified path index is the default node.
   * @param pathIndex The index of the path to check.
   * @return True if the path index is the default node, otherwise false.
   */
  bool isDefault(std::size_t pathIndex) const;

  /**
   * @brief Gets the index of the default node.
   * @return The index of the default node.
   */
  std::optional<std::size_t> defaultIndex() const;

  /**
   * @brief Gets the path corresponding to the specified path index.
   * @param pathIndex The index of the path.
   * @return The path corresponding to the specified index.
   */
  std::string_view path(std::size_t pathIndex) const;

  /**
   * @brief Gets a range of all path indices in the graph.
   * @return A range of all path indices.
   */
  std::ranges::iota_view<std::size_t, std::size_t> nodes() const;

  /**
   * @brief Gets the vector of output nodes for the specified path index.
   * @param pathIndex The index of the path.
   * @return The vector of output nodes.
   */
  std::span<const std::size_t> out(std::size_t pathIndex) const;

  /**
   * @brief Gets the vector of input nodes for the specified path index.  Note
   * that this does not include order-only dependencies.
   * @param pathIndex The index of the path.
   * @return The vector of input nodes.
   */
  std::span<const std::size_t> in(std::size_t pathIndex) const;

  /**
   * @brief Gets the index of the specified path.
   * @param path The path to be found.
   * @return The index of the path.
   */
  std::size_t getPath(std::string_view path) const;

  /**
   * @brief Gets the number of nodes in the graph.
   * @return The number of nodes.
   */
  std::size_t size() const;
};

}  // namespace trimja

#endif  // TRIMJA_GRAPH
