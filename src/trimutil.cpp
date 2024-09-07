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
#include "logreader.h"
#include "murmur_hash.h"

#include <ninja/eval_env.h>
#include <ninja/lexer.h>
#include <ninja/util.h>

#include <array>
#include <cassert>
#include <filesystem>
#include <forward_list>
#include <fstream>
#include <iostream>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace trimja {

namespace {

struct TransparentHash {
  using is_transparent = void;
  std::size_t operator()(const std::string& v) const {
    return std::hash<std::string_view>{}(v);
  }
  std::size_t operator()(std::string_view v) const {
    return std::hash<std::string_view>{}(v);
  }
};

class BasicScope {
  std::unordered_map<std::string, std::string, TransparentHash, std::equal_to<>>
      m_variables;

 public:
  BasicScope() = default;

  template <typename STRING>
  std::string_view set(std::string_view key, STRING&& value) {
    const auto [it, inserted] = m_variables.emplace(key, value);
    if (!inserted) {
      it->second = std::forward<STRING>(value);
    }
    return it->second;
  }

  bool appendValue(std::string& output, std::string_view name) const {
    const auto it = m_variables.find(name);
    if (it == m_variables.end()) {
      return false;
    } else {
      output += it->second;
      return true;
    }
  }
};

struct Rule {
  static const std::size_t bindingCount = 11;

  std::string name;
  std::array<unsigned char, bindingCount + 1>
      lookup;  // TODO: pack into uint64_t
  std::vector<EvalString> bindings;

  explicit Rule(std::string_view name) : name(name), lookup(), bindings() {
    lookup.fill(std::numeric_limits<unsigned char>::max());
  }

  std::size_t getLookupIndex(std::string_view varName) const {
    const std::array<std::string_view, bindingCount> names = {
        "command",
        "depfile",
        "dyndep",
        "description",
        "deps",
        "generator",
        "pool",
        "restat",
        "rspfile",
        "rspfile_content",
        "msvc_deps_prefix",
    };
    return std::find(names.begin(), names.end(), varName) - names.begin();
  }

  bool add(std::string_view varName, EvalString&& value) {
    const std::size_t lookupIndex = getLookupIndex(varName);
    if (lookupIndex >= bindingCount) {
      return false;
    }
    const std::size_t bindingIndex = lookup[lookupIndex];
    if (bindingIndex < bindings.size()) {
      bindings[bindingIndex] = std::move(value);
    } else {
      bindings.push_back(std::move(value));
      lookup[lookupIndex] = static_cast<unsigned char>(bindings.size() - 1);
    }
    return true;
  }

  const EvalString* lookupVar(std::string_view varName) const {
    const std::size_t bindingIndex = lookup[getLookupIndex(varName)];
    return bindingIndex < bindings.size() ? &bindings[bindingIndex] : nullptr;
  }
};

template <typename SCOPE>
struct EdgeScope {
  std::span<const std::string> ins;
  std::span<const std::string> outs;
  BasicScope local;
  SCOPE& parent;
  const Rule& rule;

  EdgeScope(SCOPE& parent,
            const Rule& rule,
            std::span<const std::string> ins,
            std::span<const std::string> outs)
      : ins(ins), outs(outs), local(), parent(parent), rule(rule) {}

  bool appendValue(std::string& output, std::string_view name) const {
    // From https://ninja-build.org/manual.html#ref_scope
    // Variable declarations indented in a build block are scoped to the build
    // block. The full lookup order for a variable expanded in a build block (or
    // the rule is uses) is:
    //   1. Special built-in variables ($in, $out).
    //   2. Build-level variables from the build block.
    //   3. Rule-level variables from the rule block (i.e. $command). (Note from
    //      the above discussion on expansion that these are expanded "late",
    //      and may make use of in-scope bindings like $in.)
    //   4. File-level variables from the file that the build line was in.
    //   5. Variables from the file that included that file using the subninja
    //      keyword.
    if (name == "in") {
      appendPaths(output, ins, ' ');
      return true;
    } else if (name == "out") {
      appendPaths(output, outs, ' ');
      return true;
    } else if (name == "in_newline") {
      appendPaths(output, ins, '\n');
      return true;
    } else {
      if (local.appendValue(output, name)) {
        return true;
      }

      if (const EvalString* value = rule.lookupVar(name)) {
        evaluate(output, *value, *this);
        return true;
      }

      return parent.appendValue(output, name);
    }
  }

 private:
  static void appendPaths(std::string& output,
                          std::span<const std::string> paths,
                          const char separator) {
    auto it = paths.begin();
    const auto end = paths.end();
    if (it == end) {
      return;
    }

    goto skipSeparator;
    for (; it != end; ++it) {
      output += separator;
    skipSeparator:
      appendEscapedString(output, *it);
    }
  }
};

template <typename SCOPE>
void evaluate(std::string& output,
              const EvalString& variable,
              const SCOPE& scope) {
  for (const auto& [string, type] : variable.parsed_) {
    if (type == EvalString::RAW) {
      output += string;
    } else {
      scope.appendValue(output, string);
    }
  }
}

class Parser {
 public:
  // All parts of the input build file.  Note that we may swap out
  // build command sections with phony commands.
  std::vector<std::string_view> m_parts;

  struct BuildCommand {
    enum Resolution {
      // Print the entire build command
      Print,

      // Create a phony command for all the (implicit) outputs
      Phony,
    };

    Resolution resolution = Phony;

    // The location of our entire build command inside `m_parts`
    std::size_t partsIndex = std::numeric_limits<std::size_t>::max();

    // The hash of the build command
    std::uint64_t hash = 0;

    // Map each output index to the string containing the
    // "build out1 out$ 2 | implicitOut3" (note no newline and no trailing `|`
    // or `:`)
    std::string_view outStr;

    // Map each output index to the string containing the validation edges
    // e.g. "|@ validation1 validation2" (note no newline and no leading
    // space)
    std::string_view validationStr;
  };

  // All build commands and default statements mentioned
  std::vector<BuildCommand> m_commands;

  // Map each output index to the index within `m_command`.  Use -1 for a
  // value that isn't an output to a build command (i.e. a source file)
  std::vector<std::size_t> m_nodeToCommand;

 private:
  std::unordered_map<std::string, Rule, TransparentHash, std::equal_to<>>
      m_rules;

  BasicScope m_fileScope;

  // Our lexer
  Lexer m_lexer;

  // Our graph
  Graph& m_graph;

 public:
  std::size_t getPathIndex(std::string_view path) {
    const std::size_t index = m_graph.addPath(std::string(path));
    if (index >= m_nodeToCommand.size()) {
      m_nodeToCommand.resize(index + 1,
                             std::numeric_limits<std::size_t>::max());
    }
    return index;
  }

  std::size_t getDefault() {
    const std::size_t index = m_graph.addDefault();
    if (index >= m_nodeToCommand.size()) {
      m_nodeToCommand.resize(index + 1,
                             std::numeric_limits<std::size_t>::max());
    }
    return index;
  }

 private:
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
    EvalString tmp;
    if (!m_lexer.ReadPath(&tmp, &err)) {
      throw std::runtime_error(err);
    }
    expectToken(Lexer::NEWLINE);
  }

  void skipRule() {
    std::string_view name;
    if (!m_lexer.ReadIdent(&name)) {
      throw std::runtime_error("Missing name for rule");
    }

    const auto [it, inserted] = m_rules.emplace(name, name);
    if (!inserted) {
      std::stringstream ss;
      ss << "Duplicate rule '" << name << "' found!" << '\0';
      throw std::runtime_error(ss.view().data());
    }

    Rule& rule = it->second;

    expectToken(Lexer::NEWLINE);
    while (m_lexer.PeekToken(Lexer::INDENT)) {
      std::string_view key;
      EvalString value;
      parseLet(key, value);
      if (!rule.add(key, std::move(value))) {
        std::stringstream ss;
        ss << "Unexpected variable '" << key << "' in rule '" << name
           << "' found!" << '\0';
        throw std::runtime_error(ss.view().data());
      }
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

  void parseLet(std::string_view& key, EvalString& value) {
    if (!m_lexer.ReadIdent(&key)) {
      throw std::runtime_error("Missing variable name");
    }

    expectToken(Lexer::EQUALS);

    std::string err;
    if (!m_lexer.ReadVarValue(&value, &err)) {
      throw std::runtime_error(err);
    }
  }

  template <typename SCOPE>
  std::size_t collectPaths(std::vector<std::string>& result,
                           SCOPE& scope,
                           std::string* err) {
    EvalString out;
    std::size_t count = 0;
    while (true) {
      out.Clear();
      if (!m_lexer.ReadPath(&out, err)) {
        throw std::runtime_error(*err);
      }
      if (out.empty()) {
        break;
      }

      evaluate(result.emplace_back(), out, scope);
      ++count;
    }

    return count;
  }

  void handleEdge(const char* start) {
    std::vector<std::string> outs;
    std::string errStorage;
    std::string* err = &errStorage;

    {
      EvalString out;
      if (!m_lexer.ReadPath(&out, err)) {
        throw std::runtime_error(*err);
      }
      while (!out.empty()) {
        // TODO: Allow bindings from the rule to be looked up on edges
        evaluate(outs.emplace_back(), out, m_fileScope);
        out.Clear();
        if (!m_lexer.ReadPath(&out, err)) {
          throw std::runtime_error(*err);
        }
      }
    }
    const std::size_t outSize = outs.size();

    if (m_lexer.PeekToken(Lexer::PIPE)) {
      // Collect implicit outs
      collectPaths(outs, m_fileScope, err);
    }

    if (outs.empty()) {
      throw std::runtime_error("Missing output paths in build command");
    }

    expectToken(Lexer::COLON);

    // Mark the outputs for later
    const std::string_view outStr(start, m_lexer.position());

    std::string_view ruleName;
    if (!m_lexer.ReadIdent(&ruleName)) {
      throw std::runtime_error("Missing rule name for build command");
    }

    const Rule& rule = [&] {
      const auto ruleIt = m_rules.find(ruleName);
      if (ruleIt == m_rules.end()) {
        throw std::runtime_error("Unable to find " + std::string(ruleName) +
                                 " rule");
      }
      return ruleIt->second;
    }();

    // Collect inputs
    std::vector<std::string> ins;
    const std::size_t inSize = collectPaths(ins, m_fileScope, err);

    // Collect implicit inputs
    if (m_lexer.PeekToken(Lexer::PIPE)) {
      collectPaths(ins, m_fileScope, err);
    }

    // Collect build-order dependencies
    if (m_lexer.PeekToken(Lexer::PIPE2)) {
      collectPaths(ins, m_fileScope, err);
    }

    // Collect validations but ignore what they are. If we include a build
    // command it will include the validation.  If that validation has a
    // required input then we include that, otherwise the validation is
    // `phony`ed out.
    std::string_view validationStr;
    const char* validationStart = m_lexer.position();
    if (m_lexer.PeekToken(Lexer::PIPEAT)) {
      std::vector<std::string> validations;
      collectPaths(validations, m_fileScope, err);
      validationStr = std::string_view(validationStart, m_lexer.position());
    }

    expectToken(Lexer::NEWLINE);

    EdgeScope scope(m_fileScope, rule, std::span(ins.data(), inSize),
                    std::span(outs.data(), outSize));

    EvalString value;
    while (m_lexer.PeekToken(Lexer::INDENT)) {
      std::string_view key;
      parseLet(key, value);
      std::string result;
      evaluate(result, value, scope);
      scope.local.set(key, std::move(result));
      value.Clear();
    }

    const std::size_t partsIndex = m_parts.size();
    m_parts.emplace_back(start, m_lexer.position());

    // Add the build command
    const std::size_t commandIndex = m_commands.size();
    BuildCommand& buildCommand = m_commands.emplace_back();
    buildCommand.partsIndex = partsIndex;
    buildCommand.validationStr = validationStr;
    buildCommand.outStr = outStr;

    // Set up the mapping from each output index to the corresponding
    // entry in `m_parts`
    std::vector<std::size_t> outIndices;
    for (const std::string& out : outs) {
      const std::size_t outIndex = getPathIndex(out);
      outIndices.push_back(outIndex);
      m_nodeToCommand[outIndex] = commandIndex;
    }

    // Add all in edges and connect them with all output edges

    for (const std::string& in : ins) {
      const std::size_t inIndex = getPathIndex(in);
      for (const std::size_t outIndex : outIndices) {
        m_graph.addEdge(inIndex, outIndex);
      }
    }

    std::string command;
    scope.appendValue(command, "command");

    buildCommand.hash = murmur_hash::hash(command.data(), command.size());
  }

  void handleDefault(const char* start) {
    std::vector<std::string> ins;
    std::string err;
    collectPaths(ins, m_fileScope, &err);
    if (ins.empty()) {
      throw std::runtime_error("Expected path");
    }

    expectToken(Lexer::NEWLINE);

    const std::size_t partsIndex = m_parts.size();
    m_parts.emplace_back(start, m_lexer.position());

    const std::size_t commandIndex = m_commands.size();
    BuildCommand& buildCommand = m_commands.emplace_back();
    buildCommand.partsIndex = partsIndex;

    const std::size_t outIndex = getDefault();
    m_nodeToCommand[outIndex] = commandIndex;
    for (const std::string& in : ins) {
      m_graph.addEdge(getPathIndex(in), outIndex);
    }
  }

 public:
  Parser(Graph& graph) : m_graph(graph) {}

  void parse(const std::filesystem::path& filename,
             std::string_view input,
             std::filesystem::path& builddir) {
    m_lexer.Start(filename.string(), input);

    while (true) {
      const char* start = m_lexer.position();
      const Lexer::Token token = m_lexer.ReadToken();
      switch (token) {
        case Lexer::POOL:
          skipPool();
          m_parts.emplace_back(start, m_lexer.position());
          break;
        case Lexer::BUILD:
          handleEdge(start);
          break;
        case Lexer::RULE:
          skipRule();
          m_parts.emplace_back(start, m_lexer.position());
          break;
        case Lexer::DEFAULT:
          handleDefault(start);
          break;
        case Lexer::IDENT: {
          m_lexer.UnreadToken();
          std::string_view key;
          EvalString value;
          parseLet(key, value);
          std::string result;
          evaluate(result, value, m_fileScope);
          m_fileScope.set(key, std::move(result));
          m_parts.emplace_back(start, m_lexer.position());
        } break;
        case Lexer::INCLUDE:
        case Lexer::SUBNINJA:
          skipInclude();
          m_parts.emplace_back(start, m_lexer.position());
          break;
        case Lexer::ERROR:
          throw std::runtime_error("Parsing error");
        case Lexer::TEOF: {
          builddir = filename;
          builddir.remove_filename();
          std::string out;
          m_fileScope.appendValue(out, "builddir");
          builddir /= out;
          return;
        }
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

  void print(std::ostream& output) const {
    std::copy(m_parts.begin(), m_parts.end(),
              std::ostream_iterator<std::string_view>(output));
  }
};

enum Requirement : char {
  // This build command is needed only by `default` and has no inputs marked
  // as required so instead of modifying the `default` statement we instead
  // create a build statement that is an empty `phony`.
  CreatePhony,

  // This build command should be printed as-is, but it doesn't require inputs
  // or outputs (e.g. `default` statement)
  None,

  // We need all inputs of this build command, but not necessarily all of the
  // outputs.
  Inputs,

  // We need both inputs and outputs of this command.
  InputsAndOutputs,
};

void markOutputsAsRequired(Graph& graph,
                           std::size_t index,
                           std::vector<Requirement>& requirement) {
  for (const std::size_t out : graph.out(index)) {
    switch (requirement[out]) {
      case Requirement::CreatePhony:
      case Requirement::Inputs:
        requirement[out] = Requirement::InputsAndOutputs;
        markOutputsAsRequired(graph, out, requirement);
        break;
      case Requirement::InputsAndOutputs:
        break;
      case Requirement::None:
        assert(!"Should not have 'None' at this point");
    }
  }
}

void markInputsAsRequired(Graph& graph,
                          std::size_t index,
                          std::vector<Requirement>& requirement) {
  for (const std::size_t in : graph.in(index)) {
    switch (requirement[in]) {
      case Requirement::CreatePhony:
        requirement[in] = Requirement::Inputs;
        markInputsAsRequired(graph, in, requirement);
        break;
      case Requirement::Inputs:
      case Requirement::InputsAndOutputs:
        break;
      case Requirement::None:
        assert(!"Should not get 'None' as an input");
    }
  }
}

void parseDepFile(const std::filesystem::path& ninjaDeps,
                  Graph& graph,
                  Parser& parser) {
  std::ifstream deps(ninjaDeps);
  DepsReader reader(deps);
  std::vector<std::size_t> lookup;
  while (true) {
    const auto record = reader.read();
    switch (record.index()) {
      case 0: {
        const PathRecordView& view = std::get<PathRecordView>(record);
        if (static_cast<std::size_t>(view.index) >= lookup.size()) {
          lookup.resize(view.index + 1);
        }
        lookup[view.index] = parser.getPathIndex(view.path);
        break;
      }
      case 1: {
        const DepsRecordView& view = std::get<DepsRecordView>(record);
        for (const std::int32_t inIndex : view.deps) {
          graph.addEdge(lookup[inIndex], lookup[view.outIndex]);
        }
        break;
      }
      case 2:
        return;
    }
  }
}

template <typename GET_HASH>
void parseLogFile(const std::filesystem::path& ninjaLog,
                  const Graph& graph,
                  std::vector<Requirement>& requirements,
                  GET_HASH&& get_hash) {
  std::ifstream deps(ninjaLog);

  // As there can be duplicate entries and subsequent entries take precedence
  // first record everything we care about and then update the graph
  std::vector<bool> seen(graph.size());
  std::vector<bool> hashMismatch(graph.size());
  for (const LogEntry& entry : LogReader(deps)) {
    const std::string& path = entry.output.string();
    if (!graph.hasPath(path)) {
      // If we don't have the path then it was since removed from the ninja
      // build file
      continue;
    }

    const std::size_t index = graph.getPath(path);
    seen[index] = true;
    hashMismatch[index] = entry.hash != get_hash(index);
  }

  // Mark all build commands that are new or have been changed as required
  for (std::size_t index = 0; index < seen.size(); ++index) {
    const bool isBuildCommand = !graph.in(index).empty();
    if (isBuildCommand && (!seen[index] || hashMismatch[index])) {
      requirements[index] = Requirement::InputsAndOutputs;
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

  // Parse the build file
  std::filesystem::path builddir;
  Parser parser(graph);
  parser.parse(ninjaFile, ninjaFileContents, builddir);

  // Add all dynamic dependencies from `.ninja_deps` to the graph
  if (const std::filesystem::path ninjaDeps = builddir / ".ninja_deps";
      std::filesystem::exists(ninjaDeps)) {
    parseDepFile(ninjaDeps, graph, parser);
  }

  std::vector<Requirement> requirements(graph.size(), Requirement::CreatePhony);

  // Look through all log entries and mark as required those build commands that
  // are either absent in the log (representing new commands that have never
  // been run) or those whose hash has changed.
  if (const std::filesystem::path ninjaLog = builddir / ".ninja_log";
      !std::filesystem::exists(ninjaLog)) {
    // If we don't have a `.ninja_log` file then either the user didn't have
    // it,which is an error, or our previous run did not include any build
    // commands. The former is far more likely so we warn the user in this case.
    std::cerr << "Unable to find " << ninjaLog << ", so including everything"
              << std::endl;
    requirements.assign(requirements.size(), Requirement::InputsAndOutputs);
  } else {
    parseLogFile(ninjaLog, graph, requirements, [&](const std::size_t index) {
      return parser.m_commands[parser.m_nodeToCommand[index]].hash;
    });
  }

  // Mark all files in `changed` as required
  for (std::string line; std::getline(changed, line);) {
    if (!graph.hasPath(line)) {
      throw std::runtime_error("Unable to find " + line + " in " +
                               ninjaFile.string());
    }
    requirements[graph.getPath(line)] = Requirement::InputsAndOutputs;
  }

  // Mark all outputs as required or not
  for (std::size_t index = 0; index < graph.size(); ++index) {
    if (requirements[index] == Requirement::InputsAndOutputs) {
      markOutputsAsRequired(graph, index, requirements);
    }
  }

  // Regardless of what the default index was set to, we set it to `None` so
  // that we don't require any of its inputs
  if (const std::size_t defaultIndex = graph.defaultIndex();
      defaultIndex != std::numeric_limits<std::size_t>::max()) {
    requirements[defaultIndex] = Requirement::None;
  }

  // Mark all inputs as required.  The only time we don't do this
  // is for the default rule since this is just a nice way for users to
  // build a set of output files and when we're using `trimja` we only
  // want to build what has changed.
  for (std::size_t index = 0; index < graph.size(); ++index) {
    switch (requirements[index]) {
      case Requirement::CreatePhony:
      case Requirement::None:
        break;
      case Requirement::Inputs:
      case Requirement::InputsAndOutputs:
        markInputsAsRequired(graph, index, requirements);
        break;
    }
  }

  // Mark all affected `BuildCommands` as needing to print them out
  for (std::size_t index = 0; index < graph.size(); ++index) {
    switch (requirements[index]) {
      case Requirement::CreatePhony:
        break;
      case Requirement::None:
      case Requirement::Inputs:
      case Requirement::InputsAndOutputs: {
        const std::size_t commandIndex = parser.m_nodeToCommand[index];
        if (commandIndex != std::numeric_limits<std::size_t>::max()) {
          parser.m_commands[commandIndex].resolution =
              Parser::BuildCommand::Print;
        }
        break;
      }
    }
  }

  // Go through all commands that need to be `phony`ed and do so
  std::forward_list<std::string> phonyStorage;
  for (const Parser::BuildCommand& command : parser.m_commands) {
    if (command.resolution == Parser::BuildCommand::Phony) {
      const std::initializer_list<std::string_view> parts = {
          command.outStr,
          command.validationStr.empty() ? "phony" : "phony ",
          command.validationStr,
          "\n",
      };
      std::string& phony = phonyStorage.emplace_front();
      phony.resize(
          std::accumulate(parts.begin(), parts.end(), phony.size(),
                          [](std::size_t size, const std::string_view part) {
                            return size + part.size();
                          }));
      [[maybe_unused]] const auto it =
          std::accumulate(parts.begin(), parts.end(), phony.begin(),
                          [](auto outIt, const std::string_view part) {
                            return std::copy(part.begin(), part.end(), outIt);
                          });
      assert(it == phony.end());
      parser.m_parts[command.partsIndex] = std::string_view{phony};
    }
  }

  parser.print(output);
}

}  // namespace trimja
