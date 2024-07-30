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

#include "lexer.h"

#include <list>
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
 private:
  enum class CommandRequirement {
    // We are unsure whether this build command is needed or not
    None,
    // This is not
    InputsOnly,
    // This build command is required and so is all commands that
    // depend on any of its outputs
    InputAndOutputs,
  };

  // Our lexer
  Lexer m_lexer;

  // The eventual output
  std::string m_output;

  // A look up from path to vertex index.
  // TODO: Change the key to `std::string_view`
  std::unordered_map<std::string, std::size_t> m_pathToIndex;

  // An adjacency list of input -> output
  std::vector<std::set<std::size_t>> m_inputToOutput;

  // An adjacency list of output -> Input
  std::vector<std::set<std::size_t>> m_outputToInput;

  // Whether an input has been required or not
  std::vector<CommandRequirement> m_state;

  // If the path has an output rule then put it in here. This points
  // to elements in 'm_unknownEdges' and we may share this with several
  // paths if the build edge has multiple outputs.
  std::vector<std::string_view*> m_outputRule;

  // This stores the build edges text that we haven't used yet
  std::list<std::string_view> m_unknownEdges;

  std::size_t getPathIndex(std::string_view path) {
    const auto [it, inserted] = m_pathToIndex.emplace(path, -1);
    if (inserted) {
      it->second = m_inputToOutput.size();
      m_inputToOutput.emplace_back();
      m_outputToInput.emplace_back();
      m_state.emplace_back();
      m_outputRule.emplace_back(&alwaysEmpty);
    }
    return it->second;
  }

  void markAsRequired(std::size_t index) {
    // Do nothing if we already knew we were required
    if (m_state[index] == CommandRequirement::InputAndOutputs) {
      return;
    }
    // Mark ourselves as required
    m_state[index] = CommandRequirement::InputAndOutputs;

    // Print out the rule that generates us (note this may aleady
    // have been printed if that rule created multiple outputs).
    // Reset to "" so that no one else will output this rule again
    m_output += std::exchange(*m_outputRule[index], "");

    // Mark all dependent edges as required
    for (std::size_t outIndex : m_inputToOutput[index]) {
      markAsRequired(outIndex);
    }
  }

  void requireInputs(std::size_t index) {
    // TODO, merge with markAsRequired and early exit when we have
    // an empty string in `outputRule` as if we have been printed, so
    // has all our dependencies? TODO: Check this

    // Do nothing if we already knew we were required
    // if (m_state[index] >= CommandRequirement::InputsOnly) {
    // return;
    //}

    // Print out the rule that generates us (note this may aleady
    // have been printed if that rule created multiple outputs).
    // Reset to "" so that no one else will output this rule again
    m_output += std::exchange(*m_outputRule[index], "");

    // Mark all dependent edges as required
    for (std::size_t inIndex : m_outputToInput[index]) {
      requireInputs(inIndex);
    }
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
    expectToken(Lexer::EQUALS);
    std::string err;
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

    std::string_view* text =
        &m_unknownEdges.emplace_back(start, m_lexer.position());
    for (const std::string& in : ins) {
      const std::size_t inIndex = getPathIndex(in);

      // Set up the adjacency list
      for (const std::string& out : outs) {
        const std::size_t outIndex = getPathIndex(out);
        m_inputToOutput[inIndex].emplace(outIndex);
        m_outputToInput[outIndex].emplace(inIndex);
        m_outputRule[outIndex] = text;
      }
    }

    // If any of the inputs requires outputs, then we are required
    // and we need to include ourselves, mark ourselves as
    // outputs required and go mark any dependents as such,
    // we also need to print any other inputs we have (but not mark
    // them as required), and recursively print their inputs etc.

    for (const std::string& in : ins) {
      const std::size_t inIndex = getPathIndex(in);

      // If any edge is required, mark all the outputs
      // as required and include the build edge text
      if (m_state[inIndex] == CommandRequirement::InputAndOutputs) {
        for (const std::string& out : outs) {
          markAsRequired(getPathIndex(out));
        }
        for (const std::string& in2 : ins) {
          requireInputs(getPathIndex(in2));
        }
        return;
      }
    }
  }

  void handleDefault(const char* start) {
    std::string errStorage;
    std::string* err = &errStorage;
    std::string path;
    if (!m_lexer.ReadPath(&path, err)) {
      throw std::runtime_error(*err);
    }
    if (path.empty()) {
      throw std::runtime_error("Expected path");
    }

    while (m_lexer.ReadPath(&path, err)) {
      if (path.empty()) {
        throw std::runtime_error("Empty path");
      }
    }

    expectToken(Lexer::NEWLINE);
  }

 public:
  Parser() = default;

  void markRequired(std::string_view path) {
    m_state[getPathIndex(path)] = CommandRequirement::InputAndOutputs;
  }

  std::string trim(std::string_view filename, std::string_view input) {
    m_lexer.Start(filename, input);

    while (true) {
      const char* start = m_lexer.position();
      const Lexer::Token token = m_lexer.ReadToken();
      switch (token) {
        case Lexer::POOL:
          skipPool();
          m_output.append(start, m_lexer.position());
          break;
        case Lexer::BUILD:
          handleEdge(start);
          break;
        case Lexer::RULE:
          skipRule();
          m_output.append(start, m_lexer.position());
          break;
        case Lexer::DEFAULT:
          handleDefault(start);
          break;
        case Lexer::IDENT:
          m_lexer.UnreadToken();
          skipLet();
          m_output.append(start, m_lexer.position());
          break;
        case Lexer::INCLUDE:
        case Lexer::SUBNINJA:
          skipInclude();
          m_output.append(start, m_lexer.position());
          break;
        case Lexer::ERROR:
          throw std::runtime_error("Parsing error");
        case Lexer::TEOF:
          return std::exchange(m_output, "");
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
};

}  // namespace

void TrimUtil::trim(std::ostream& output,
                    std::string_view filename,
                    std::istream& ninja,
                    std::istream& changed) {
  Parser parser;
  for (std::string line; std::getline(changed, line);) {
    parser.markRequired(line);
  }

  // As we use `view()` and not `str()`, append '\0' to make sure it
  // is null terminated.
  std::stringstream buffer;
  buffer << ninja.rdbuf() << '\0';
  output << parser.trim(filename, buffer.view());
}

}  // namespace trimja
