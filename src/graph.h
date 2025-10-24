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
#include "indexinto.h"

#include <boost/boost_unordered.hpp>
#include <gch/small_vector.hpp>

#include <optional>
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

 public:
  using Node = IndexInto<Graph>;

 private:
  // A look up from path to vertex index.
  boost::unordered_flat_map<fixed_string, Node, PathHash, PathEqual>
      m_pathToNode;

  // An adjacency list of input -> output
  std::vector<gch::small_vector<Node>> m_inputToOutput;

  // An adjacency list of output -> Input
  std::vector<gch::small_vector<Node>> m_outputToInput;

  // Names of paths (this points to the keys in `m_pathToNode`, which is always
  // valid since `fixed_string` has no small-string optimization and always
  // allocates on the heap.
  std::vector<std::string_view> m_path;

  std::optional<Node> m_defaultNode;

 public:
  /**
   * @brief Constructs an empty Graph.
   */
  Graph();

  // Note that the move constructor is deleted because it would
  // invalidate the container pointers of the `Node` class.  If this is needed
  // in the future then the contents of `Graph` should be put inside a
  // `std::unique_ptr` - at least in debug builds.
  Graph(Graph&&) = delete;
  Graph(const Graph&) = delete;
  Graph& operator=(Graph&&) = delete;
  Graph& operator=(const Graph&) = delete;

  /**
   * @brief Adds the path to the graph if it isn't already present and returns
   * the corresponding node.
   * @param path The path to be added. It will be normalized.
   * @return The node corresponding to the added path.
   */
  Node addPath(std::string&& path);

  /**
   * @brief Adds a normalized path to the graph if it isn't already present and
   * returns the corresponding node.
   * @param path The normalized path to be added.
   * @return The node corresponding to the added path.
   */
  Node addNormalizedPath(std::string_view path);

  /**
   * @brief Finds the node of the specified path if it exists.
   * @param path The path to be found. It will be normalized.
   * @return The node of the path if found, otherwise std::nullopt.
   */
  std::optional<Node> findPath(std::string&& path) const;

  /**
   * @brief Finds the node of the specified normalized path if it exists.
   * @param path The normalized path to be found.
   * @return The node of the path if found, otherwise std::nullopt.
   */
  std::optional<Node> findNormalizedPath(std::string_view path) const;

  /**
   * @brief Adds a default node to the graph.
   * @pre There is no default node already present.
   * @return The default node.
   */
  Node addDefault();

  /**
   * @brief Adds bidirectional edge between the specified input and output
   * nodes.
   * @param in The input node.
   * @param out The output node.
   */
  void addEdge(Node in, Node out);

  /**
   * @brief Adds an one-way edge from the specified input to output node.
   * @param in The input node.
   * @param out The output node.
   */
  void addOneWayEdge(Node in, Node out);

  /**
   * @brief Checks if the specified node is the default node.
   * @param node The node to check.
   * @return Whether the node is the default.
   */
  bool isDefault(Node node) const;

  /**
   * @brief Gets the default node if it exists.
   * @return The optional default node.
   */
  std::optional<Node> getDefault() const;

  /**
   * @brief Gets the path corresponding to the specified node.
   * @param node The node to get the path of.
   * @return The path corresponding to the specified node.
   */
  std::string_view path(Node node) const;

  /**
   * @brief Gets the vector of output nodes for the specified node.
   * @param node The node to get the outputs of.
   * @return The vector of output nodes.
   */
  const gch::small_vector<Node>& out(Node node) const;

  /**
   * @brief Gets the vector of input nodes for the specified node.  Note
   * that this does not include order-only dependencies.
   * @param node The node to get the inputs of.
   * @return The vector of input nodes.
   */
  const gch::small_vector<Node>& in(Node node) const;

  /**
   * @brief Gets the number of nodes in the graph.
   * @return The number of nodes.
   */
  std::size_t size() const;

  /**
   * @brief Return a range of all nodes in this graph.
   * @return A range of all nodes.
   */
  IndexIntoRange<Node> nodes() const;
};

}  // namespace trimja

#endif  // TRIMJA_GRAPH
