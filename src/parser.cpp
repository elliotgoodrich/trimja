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

#include "parser.h"

#include "graph.h"
#include "murmur_hash.h"

#include <ninja/lexer.h>
#include <ninja/util.h>

#include <cassert>
#include <fstream>
#include <iostream>
#include <span>

namespace trimja {

namespace {

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

class ParserImp {
  BuildContext* m_ctx = nullptr;

  // Our lexer
  Lexer m_lexer;

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

  void parseInclude() {
    EvalString pathEval;
    std::string err;
    if (!m_lexer.ReadPath(&pathEval, &err)) {
      throw std::runtime_error(err);
    }
    std::string path;
    evaluate(path, pathEval, m_ctx->fileScope);

    if (!std::filesystem::exists(path)) {
      throw std::runtime_error(std::format("Unable to find {}!", path));
    }
    std::stringstream ninjaCopy;
    std::ifstream ninja(path);
    ninjaCopy << ninja.rdbuf();
    m_ctx->fileContents.push_front(ninjaCopy.str());

    ParserUtil::parse(*m_ctx, path, m_ctx->fileContents.front());

    expectToken(Lexer::NEWLINE);
  }

  void skipRule(const char* start) {
    std::string_view name;
    if (!m_lexer.ReadIdent(&name)) {
      throw std::runtime_error("Missing name for rule");
    }

    const std::size_t ruleIndex = m_ctx->rules.size();
    const auto [ruleIt, inserted] = m_ctx->ruleLookup.emplace(name, ruleIndex);
    if (!inserted) {
      std::stringstream ss;
      ss << "Duplicate rule '" << name << "' found!" << '\0';
      throw std::runtime_error(ss.view().data());
    }

    Rule& rule = m_ctx->rules.emplace_back(ruleIt->first);

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

    const std::size_t partsIndex = m_ctx->parts.size();
    m_ctx->parts.emplace_back(start, m_lexer.position());
    rule.partsIndex = partsIndex;
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

      std::string& output = result.emplace_back();
      evaluate(output, out, scope);
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
        std::string& output = outs.emplace_back();
        evaluate(output, out, m_ctx->fileScope);
        out.Clear();
        if (!m_lexer.ReadPath(&out, err)) {
          throw std::runtime_error(*err);
        }
      }
    }
    const std::size_t outSize = outs.size();

    if (m_lexer.PeekToken(Lexer::PIPE)) {
      // Collect implicit outs
      collectPaths(outs, m_ctx->fileScope, err);
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

    const std::size_t ruleIndex = [&] {
      const auto ruleIt = m_ctx->ruleLookup.find(ruleName);
      if (ruleIt == m_ctx->ruleLookup.end()) {
        throw std::runtime_error("Unable to find " + std::string(ruleName) +
                                 " rule");
      }
      return ruleIt->second;
    }();

    // Collect inputs
    std::vector<std::string> ins;
    const std::size_t inSize = collectPaths(ins, m_ctx->fileScope, err);

    // Collect implicit inputs
    if (m_lexer.PeekToken(Lexer::PIPE)) {
      collectPaths(ins, m_ctx->fileScope, err);
    }

    // Collect build-order dependencies
    if (m_lexer.PeekToken(Lexer::PIPE2)) {
      collectPaths(ins, m_ctx->fileScope, err);
    }

    // Collect validations but ignore what they are. If we include a build
    // command it will include the validation.  If that validation has a
    // required input then we include that, otherwise the validation is
    // `phony`ed out.
    std::string_view validationStr;
    const char* validationStart = m_lexer.position();
    if (m_lexer.PeekToken(Lexer::PIPEAT)) {
      std::vector<std::string> validations;
      collectPaths(validations, m_ctx->fileScope, err);
      validationStr = std::string_view(validationStart, m_lexer.position());
    }

    expectToken(Lexer::NEWLINE);

    EdgeScope scope(m_ctx->fileScope, m_ctx->rules[ruleIndex],
                    std::span(ins.data(), inSize),
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

    const std::size_t partsIndex = m_ctx->parts.size();
    m_ctx->parts.emplace_back(start, m_lexer.position());

    // Add the build command
    const std::size_t commandIndex = m_ctx->commands.size();
    BuildCommand& buildCommand = m_ctx->commands.emplace_back();
    buildCommand.partsIndex = partsIndex;
    buildCommand.validationStr = validationStr;
    buildCommand.outStr = outStr;
    buildCommand.ruleIndex = ruleIndex;

    // Add outputs to the graph and link to the build command
    std::vector<std::size_t> outIndices;
    for (std::string& out : outs) {
      const std::size_t outIndex = m_ctx->getPathIndex(out);
      outIndices.push_back(outIndex);
      m_ctx->nodeToCommand[outIndex] = commandIndex;
    }

    // Add inputs to the graph and add the edges to the graph
    for (std::string& in : ins) {
      const std::size_t inIndex = m_ctx->getPathIndex(in);
      for (const std::size_t outIndex : outIndices) {
        m_ctx->graph.addEdge(inIndex, outIndex);
      }
    }

    {
      std::string command;
      scope.appendValue(command, "command");
      std::string rspcontent;
      scope.appendValue(rspcontent, "rspfile_content");
      if (!rspcontent.empty()) {
        command += ";rspfile=";
        command += rspcontent;
      }
      buildCommand.hash = murmur_hash::hash(command.data(), command.size());
    }
  }

  void handleDefault(const char* start) {
    std::vector<std::string> ins;
    std::string err;
    collectPaths(ins, m_ctx->fileScope, &err);
    if (ins.empty()) {
      throw std::runtime_error("Expected path");
    }

    expectToken(Lexer::NEWLINE);

    const std::size_t partsIndex = m_ctx->parts.size();
    m_ctx->parts.emplace_back(start, m_lexer.position());

    const std::size_t commandIndex = m_ctx->commands.size();
    BuildCommand& buildCommand = m_ctx->commands.emplace_back();
    buildCommand.partsIndex = partsIndex;
    buildCommand.ruleIndex = BuildContext::defaultIndex;

    const std::size_t outIndex = m_ctx->getDefault();
    m_ctx->nodeToCommand[outIndex] = commandIndex;
    for (std::string& in : ins) {
      m_ctx->graph.addEdge(m_ctx->getPathIndex(in), outIndex);
    }
  }

 public:
  ParserImp() = default;

  void parse(BuildContext& ctx,
             const std::filesystem::path& filename,
             std::string_view ninjaFileContents) {
    m_ctx = &ctx;
    m_lexer.Start(filename.string(), ninjaFileContents);

    while (true) {
      const char* start = m_lexer.position();
      const Lexer::Token token = m_lexer.ReadToken();
      switch (token) {
        case Lexer::POOL:
          skipPool();
          m_ctx->parts.emplace_back(start, m_lexer.position());
          break;
        case Lexer::BUILD:
          handleEdge(start);
          break;
        case Lexer::RULE:
          skipRule(start);
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
          evaluate(result, value, m_ctx->fileScope);
          m_ctx->fileScope.set(key, std::move(result));
          m_ctx->parts.emplace_back(start, m_lexer.position());
        } break;
        case Lexer::INCLUDE:
          parseInclude();
          break;
        case Lexer::SUBNINJA:
          throw std::runtime_error("subninja not yet supported");
        case Lexer::ERROR:
          throw std::runtime_error("Parsing error");
        case Lexer::TEOF: {
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
};

}  // namespace

Rule::Rule(std::string_view name) : name(name), lookup(), bindings() {
  lookup.fill(std::numeric_limits<unsigned char>::max());
}

std::size_t Rule::getLookupIndex(std::string_view varName) {
  return std::find(reserved.begin(), reserved.end(), varName) -
         reserved.begin();
}

bool Rule::add(std::string_view varName, EvalString&& value) {
  const std::size_t lookupIndex = getLookupIndex(varName);
  if (lookupIndex == reserved.size()) {
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

const EvalString* Rule::lookupVar(std::string_view varName) const {
  const std::size_t bindingIndex = lookup[getLookupIndex(varName)];
  return bindingIndex < bindings.size() ? &bindings[bindingIndex] : nullptr;
}

std::string_view BasicScope::set(std::string_view key, std::string&& value) {
  // `operator[]` does not support `is_transparent` and `emplace` may or
  // may not move from `value` so this is the best way to avoid allocations
  return m_variables.emplace(key, "").first->second = std::move(value);
}

bool BasicScope::appendValue(std::string& output, std::string_view name) const {
  const auto it = m_variables.find(name);
  if (it == m_variables.end()) {
    return false;
  } else {
    output += it->second;
    return true;
  }
}

bool BuildContext::isBuiltInRule(std::size_t ruleIndex) {
  static_assert(phonyIndex == 0);
  static_assert(defaultIndex == 1);
  return ruleIndex < 2;
}

BuildContext::BuildContext() {
  // Push back an empty part for the built-in rules
  for (const auto builtIn : {"phony", "default"}) {
    const std::size_t partsIndex = parts.size();
    const std::size_t ruleIndex = rules.size();
    parts.emplace_back("");
    const auto ruleIt = ruleLookup.emplace(builtIn, ruleIndex).first;
    rules.emplace_back(ruleIt->first).partsIndex = partsIndex;
  }
  assert(rules[BuildContext::phonyIndex].name == "phony");
  assert(rules[BuildContext::defaultIndex].name == "default");
}

std::size_t BuildContext::getPathIndex(std::string& path) {
  const std::size_t index = graph.addPath(path);
  if (index >= nodeToCommand.size()) {
    nodeToCommand.resize(index + 1, std::numeric_limits<std::size_t>::max());
  }
  return index;
}

std::size_t BuildContext::getPathIndexForNormalized(std::string_view path) {
  const std::size_t index = graph.addNormalizedPath(path);
  if (index >= nodeToCommand.size()) {
    nodeToCommand.resize(index + 1, std::numeric_limits<std::size_t>::max());
  }
  return index;
}

std::size_t BuildContext::getDefault() {
  const std::size_t index = graph.addDefault();
  if (index >= nodeToCommand.size()) {
    nodeToCommand.resize(index + 1, std::numeric_limits<std::size_t>::max());
  }
  return index;
}

void ParserUtil::parse(BuildContext& ctx,
                       const std::filesystem::path& ninjaFile,
                       std::string_view ninjaFileContents) {
  ParserImp parser;
  parser.parse(ctx, ninjaFile, ninjaFileContents);
}

}  // namespace trimja
