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

#include "trimutil.h"

#include "depsreader.h"
#include "graph.h"

#include <ninja/lexer.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <list>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace trimja {

namespace {

static std::string_view alwaysEmpty = "";

class Parser {
 public:
  // Split up the parts of the build file and whether we need to print it
  std::vector<std::pair<std::string_view, bool>> m_parts;

 private:
  // Map each output index to the index within `m_parts`
  std::vector<std::optional<std::size_t>> m_nodeToParts;

  // Our lexer
  Lexer m_lexer;

  // Our graph
  Graph& m_graph;

  std::size_t getPathIndex(std::string_view path) {
    const std::size_t index = m_graph.addPath(std::string(path));
    if (index >= m_nodeToParts.size()) {
      m_nodeToParts.resize(index + 1);
    }
    return index;
  }

  void expectToken(Lexer::Token expected) {
    const Lexer::Token token = m_lexer.ReadToken();
    if (token != expected) {
      std::stringstream msg;
      msg << "Expected " << Lexer::TokenName(expected) << " but got "
          << Lexer::TokenName(token) << '\0';
      throw std::runtime_error(msg.view().data());
    }
  }

  void skipPool() {
    if (!m_lexer.SkipIdent()) {
      throw std::runtime_error("Missing name for pool");
    }

    expectToken(Lexer::NEWLINE);
    while (m_lexer.PeekToken(Lexer::INDENT)) {
      skipLet();
    }
  }

  void skipInclude() {
    std::string err;
    std::string tmp;
    if (!m_lexer.ReadPath(&tmp, &err)) {
      throw std::runtime_error(err);
    }
    expectToken(Lexer::NEWLINE);
  }

  void skipRule() {
    if (!m_lexer.SkipIdent()) {
      throw std::runtime_error("Missing name for rule");
    }

    expectToken(Lexer::NEWLINE);
    while (m_lexer.PeekToken(Lexer::INDENT)) {
      skipLet();
    }
  }

  void skipLet() {
    if (!m_lexer.SkipIdent()) {
      throw std::runtime_error("Missing variable name");
    }

    std::string err;
    expectToken(Lexer::EQUALS);
    if (!m_lexer.SkipVarValue(&err)) {
      throw std::runtime_error(err);
    }
  }

  void collectPaths(std::vector<std::string>& result, std::string* err) {
    while (true) {
      std::string out;
      if (!m_lexer.ReadPath(&out, err)) {
        throw std::runtime_error(*err);
      }
      if (out.empty()) {
        return;
      }
      result.push_back(std::move(out));
    }
  }

  void handleEdge(const char* start) {
    std::vector<std::string> outs;
    std::string errStorage;
    std::string* err = &errStorage;

    {
      std::string out;
      if (!m_lexer.ReadPath(&out, err)) {
        throw std::runtime_error(*err);
      }
      while (!out.empty()) {
        outs.push_back(out);
        out.clear();
        if (!m_lexer.ReadPath(&out, err)) {
          throw std::runtime_error(*err);
        }
      }
    }

    if (m_lexer.PeekToken(Lexer::PIPE)) {
      // Collect implicit outs
      collectPaths(outs, err);
    }

    if (outs.empty()) {
      throw std::runtime_error("Missing output paths in build command");
    }

    expectToken(Lexer::COLON);
    if (!m_lexer.SkipIdent()) {
      throw std::runtime_error("Missing name for build command");
    }

    // Collect inputs
    std::vector<std::string> ins;
    collectPaths(ins, err);

    // Collect implicit inputs
    if (m_lexer.PeekToken(Lexer::PIPE)) {
      collectPaths(ins, err);
    }

    // Collect build-order dependencies
    if (m_lexer.PeekToken(Lexer::PIPE2)) {
      collectPaths(ins, err);
    }

    // Collect validations (but at the moment we don't support validations)
    if (m_lexer.PeekToken(Lexer::PIPEAT)) {
      std::vector<std::string> validations;
      collectPaths(validations, err);
    }

    expectToken(Lexer::NEWLINE);
    while (m_lexer.PeekToken(Lexer::IDENT)) {
      skipLet();
    }

    // Add the build command to `m_parts`
    const std::size_t partsIndex = m_parts.size();
    m_parts.emplace_back(std::piecewise_construct,
                         std::make_tuple(start, m_lexer.position()),
                         std::make_tuple(false));

    // Set up the mapping from each output index to the corresponding
    // entry in `m_parts`
    std::vector<std::size_t> outIndices;
    for (const std::string& out : outs) {
      const std::size_t outIndex = getPathIndex(out);
      outIndices.push_back(outIndex);
      if (outIndex >= m_nodeToParts.size()) {
        m_nodeToParts.resize(outIndex + 1);
      }
      m_nodeToParts[outIndex] = partsIndex;
    }

    // Add all in edges and connect them with all output edges
    for (const std::string& in : ins) {
      const std::size_t inIndex = getPathIndex(in);
      for (const std::size_t outIndex : outIndices) {
        m_graph.addEdge(inIndex, outIndex);
      }
    }
  }

  void handleDefault(const char* start) {
    std::vector<std::string> ins;
    std::string err;
    collectPaths(ins, &err);
    if (ins.empty()) {
      throw std::runtime_error("Expected path");
    }

    expectToken(Lexer::NEWLINE);

    // Add the build command to `m_parts`
    const std::size_t partsIndex = m_parts.size();
    m_parts.emplace_back(std::piecewise_construct,
                         std::make_tuple(start, m_lexer.position()),
                         std::make_tuple(false));

    const std::size_t outIndex = m_graph.addDefault();
    if (outIndex >= m_nodeToParts.size()) {
      m_nodeToParts.resize(outIndex + 1);
    }
    m_nodeToParts[outIndex] = partsIndex;
    for (const std::string& in : ins) {
      m_graph.addEdge(getPathIndex(in), outIndex);
    }
  }

 public:
  Parser(Graph& graph) : m_graph(graph) {}

  void parse(std::string_view filename, std::string_view input) {
    m_lexer.Start(filename, input);

    std::string builddir;

    while (true) {
      const char* start = m_lexer.position();
      const Lexer::Token token = m_lexer.ReadToken();
      switch (token) {
        case Lexer::POOL:
          skipPool();
          m_parts.emplace_back(std::piecewise_construct,
                               std::make_tuple(start, m_lexer.position()),
                               std::make_tuple(true));
          break;
        case Lexer::BUILD:
          handleEdge(start);
          break;
        case Lexer::RULE:
          skipRule();
          m_parts.emplace_back(std::piecewise_construct,
                               std::make_tuple(start, m_lexer.position()),
                               std::make_tuple(true));
          break;
        case Lexer::DEFAULT:
          handleDefault(start);
          break;
        case Lexer::IDENT:
          m_lexer.UnreadToken();
          skipLet();
          m_parts.emplace_back(std::piecewise_construct,
                               std::make_tuple(start, m_lexer.position()),
                               std::make_tuple(true));
          break;
        case Lexer::INCLUDE:
        case Lexer::SUBNINJA:
          skipInclude();
          m_parts.emplace_back(std::piecewise_construct,
                               std::make_tuple(start, m_lexer.position()),
                               std::make_tuple(true));
          break;
        case Lexer::ERROR:
          throw std::runtime_error("Parsing error");
        case Lexer::TEOF:
          return;
        case Lexer::NEWLINE:
          break;
        default: {
          std::stringstream msg;
          msg << "Unexpected token " << Lexer::TokenName(token) << '\0';
          throw std::runtime_error(msg.view().data());
        }
      }
    }
    throw std::logic_error("Not reachable");
  }

  void markForPrinting(std::size_t index) {
    if (m_nodeToParts[index].has_value()) {
      m_parts[*m_nodeToParts[index]].second = true;
    }
  }
};

enum Requirement : char {
  Unknown = 0b00,
  Inputs = 0b01,
  InputsAndOutputs = 0b11,

};

Requirement& operator&=(Requirement& lhs, const Requirement& rhs) {
  lhs = static_cast<Requirement>(static_cast<int>(lhs) & static_cast<int>(rhs));
  return lhs;
}

void markOutputsAsRequired(Graph& graph,
                           std::size_t index,
                           std::vector<Requirement>& requirement) {
  for (const std::size_t out : graph.out(index)) {
    if (requirement[out] != Requirement::InputsAndOutputs) {
      requirement[out] = Requirement::InputsAndOutputs;
      markOutputsAsRequired(graph, out, requirement);
    }
  }
}

void markInputsAsRequired(Graph& graph,
                          std::size_t index,
                          std::vector<Requirement>& requirement) {
  for (const std::size_t in : graph.in(index)) {
    if (requirement[in] == Requirement::Unknown) {
      requirement[in] = Requirement::Inputs;
      markInputsAsRequired(graph, in, requirement);
    }
  }
}

}  // namespace

void TrimUtil::trim(std::ostream& output,
                    const std::filesystem::path& ninjaFile,
                    const std::string& ninjaFileContents,
                    std::istream& changed) {
  // Add all our of changes files to the graph and mark them as required
  Graph graph;
  for (std::string line; std::getline(changed, line);) {
    graph.addPath(line);
  }
  std::vector<Requirement> requirements(graph.size(),
                                        Requirement::InputsAndOutputs);

  Parser parser(graph);
  std::filesystem::path ninjaDeps = ninjaFile;
  ninjaDeps.replace_filename(".ninja_deps");
  if (std::filesystem::exists(ninjaDeps)) {
    std::ifstream deps(ninjaDeps.string());
    DepsReader reader(deps);
    std::vector<std::size_t> lookup;
    bool exit = false;
    while (!exit) {
      const auto record = reader.read();
      switch (record.index()) {
        case 0: {
          const PathRecordView& view = std::get<PathRecordView>(record);
          if (view.index >= lookup.size()) {
            lookup.resize(view.index + 1);
          }
          lookup[view.index] = graph.addPath(std::string(view.path));
          break;
        }
        case 1: {
          const DepsRecordView& view = std::get<DepsRecordView>(record);
          for (const std::int32_t dep : view.deps) {
            graph.addEdge(lookup[view.outIndex], lookup[dep]);
          }
          break;
        }
        case 2: {
          exit = true;
          break;
        }
      }
    }
  }

  parser.parse(ninjaFile.string(), ninjaFileContents);

  requirements.resize(graph.size(), Requirement::Unknown);

  // Mark all outputs as required or not
  for (std::size_t index = 0; index < graph.size(); ++index) {
    if (requirements[index] == Requirement::InputsAndOutputs) {
      markOutputsAsRequired(graph, index, requirements);
    }
  }

  // Mark all inputs as required or not
  for (std::size_t index = 0; index < graph.size(); ++index) {
    if (requirements[index] & Requirement::Inputs) {
      markInputsAsRequired(graph, index, requirements);
    }
  }

  for (std::size_t index = 0; index < graph.size(); ++index) {
    if (requirements[index] != Unknown) {
      parser.markForPrinting(index);
    }
  }

  // DFS through our graph and then call `markForPrinting` on each required node
  // then output all parts to
  for (const auto& [text, required] : parser.m_parts) {
    if (required) {
      output << text;
    }
  }

  output.flush();
}

}  // namespace trimja
