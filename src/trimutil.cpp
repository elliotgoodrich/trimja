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

#include "basicscope.h"
#include "depsreader.h"
#include "edgescope.h"
#include "graph.h"
#include "logreader.h"
#include "manifestparser.h"
#include "murmur_hash.h"
#include "rule.h"
#include "transparenthash.h"

#include <ninja/eval_env.h>
#include <ninja/util.h>

#include <cassert>
#include <forward_list>
#include <fstream>
#include <iostream>
#include <numeric>
#include <ranges>

namespace trimja {

namespace {

template <typename RANGE>
void consume(RANGE&& range) {
  for ([[maybe_unused]] auto&& r : range) {
  }
}

struct BuildCommand {
  enum Resolution {
    // Print the entire build command
    Print,

    // Create a phony command for all the (implicit) outputs
    Phony,
  };

  Resolution resolution = Phony;

  // The location of our entire build command inside `BuildContext::parts`
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

  // The index of the rule into `BuildContext::rules`
  std::size_t ruleIndex = std::numeric_limits<std::size_t>::max();
};

struct BuildContext {
  // The indexes of the built-in rules within `rules`
  static const std::size_t phonyIndex = 0;
  static const std::size_t defaultIndex = 1;

  // An optional storage for the contents for the input ninja files.  This is
  // useful when processing `include` and `subninja` and we need to extend the
  // lifetime of the file contents until all parsing has finished
  std::forward_list<std::string> fileContents;

  // All parts of the input build file, indexing into an element of
  // `fileContents`.  Note that we may swap out build command sections with
  // phony commands.
  std::vector<std::string_view> parts;

  // All build commands and default statements mentioned
  std::vector<BuildCommand> commands;

  // Map each output index to the index within `command`.  Use -1 for a
  // value that isn't an output to a build command (i.e. a source file)
  std::vector<std::size_t> nodeToCommand;

  // Our list of rules along with their index into `m_parts`
  std::vector<std::pair<Rule, std::size_t>> rules;

  // Our rules keyed by name
  std::unordered_map<std::string_view,
                     std::size_t,
                     TransparentHash,
                     std::equal_to<>>
      ruleLookup;

  // Our top-level variables
  BasicScope fileScope;

  // Our graph
  Graph graph;

  // Return whether `ruleIndex` is a built-in rule (i.e. `default` or `phony`)
  static bool isBuiltInRule(std::size_t ruleIndex) {
    static_assert(phonyIndex == 0);
    static_assert(defaultIndex == 1);
    return ruleIndex < 2;
  }

  BuildContext() {
    // Push back an empty part for the built-in rules
    for (const auto builtIn : {"phony", "default"}) {
      const std::size_t partsIndex = parts.size();
      const std::size_t ruleIndex = rules.size();
      parts.emplace_back("");
      const auto ruleIt = ruleLookup.emplace(builtIn, ruleIndex).first;
      rules.emplace_back(ruleIt->first, partsIndex);
    }
    assert(rules[BuildContext::phonyIndex].first.name() == "phony");
    assert(rules[BuildContext::defaultIndex].first.name() == "default");
  }

  std::size_t getPathIndex(std::string& path) {
    const std::size_t index = graph.addPath(path);
    if (index >= nodeToCommand.size()) {
      nodeToCommand.resize(index + 1, std::numeric_limits<std::size_t>::max());
    }
    return index;
  }

  std::size_t getPathIndexForNormalized(std::string_view path) {
    const std::size_t index = graph.addNormalizedPath(path);
    if (index >= nodeToCommand.size()) {
      nodeToCommand.resize(index + 1, std::numeric_limits<std::size_t>::max());
    }
    return index;
  }

  std::size_t getDefault() {
    const std::size_t index = graph.addDefault();
    if (index >= nodeToCommand.size()) {
      nodeToCommand.resize(index + 1, std::numeric_limits<std::size_t>::max());
    }
    return index;
  }

  void parse(const std::filesystem::path& ninjaFile,
             const std::string& ninjaFileContents) {
    for (auto&& part : ManifestReader(ninjaFile, ninjaFileContents)) {
      std::visit(*this, part);
    }
  }

  void operator()(PoolReader& r) {
    [[maybe_unused]] const std::string_view name = r.name();
    consume(r.variables());
    parts.emplace_back(r.start(), r.position());
  }

  void operator()(BuildReader& r) {
    std::vector<std::string> outs;
    auto evaluatePath = [&](const EvalString& path) {
      std::string result;
      evaluate(result, path, fileScope);
      return result;
    };

    for (const EvalString& path : r.out()) {
      outs.push_back(evaluatePath(path));
    }
    if (outs.empty()) {
      throw std::runtime_error("Missing output paths in build command");
    }
    const std::size_t outSize = outs.size();
    for (const EvalString& path : r.implicitOut()) {
      outs.push_back(evaluatePath(path));
    }

    // Mark the outputs for later
    const std::string_view outStr(r.start(), r.position());

    std::string_view ruleName = r.name();

    const std::size_t ruleIndex = [&] {
      const auto ruleIt = ruleLookup.find(ruleName);
      if (ruleIt == ruleLookup.end()) {
        throw std::runtime_error("Unable to find " + std::string(ruleName) +
                                 " rule");
      }
      return ruleIt->second;
    }();

    // Collect inputs
    std::vector<std::string> ins;
    for (const EvalString& path : r.in()) {
      ins.push_back(evaluatePath(path));
    }
    const std::size_t inSize = ins.size();

    for (const EvalString& path : r.implicitIn()) {
      ins.push_back(evaluatePath(path));
    }
    for (const EvalString& path : r.orderOnlyDeps()) {
      ins.push_back(evaluatePath(path));
    }

    // Collect validations but ignore what they are. If we include a build
    // command it will include the validation.  If that validation has a
    // required input then we include that, otherwise the validation is
    // `phony`ed out.
    const char* validationStart = r.position();
    consume(r.validations());
    std::string_view validationStr(validationStart, r.position());

    EdgeScope scope(fileScope, rules[ruleIndex].first,
                    std::span(ins.data(), inSize),
                    std::span(outs.data(), outSize));

    for (VariableReader v : r.variables()) {
      const std::string_view name = v.name();
      std::string result;
      evaluate(result, v.value(), scope);
      scope.set(name, std::move(result));
    }

    const std::size_t partsIndex = parts.size();
    parts.emplace_back(r.start(), r.position());

    // Add the build command
    const std::size_t commandIndex = commands.size();
    BuildCommand& buildCommand = commands.emplace_back();
    buildCommand.partsIndex = partsIndex;
    buildCommand.validationStr = validationStr;
    buildCommand.outStr = outStr;
    buildCommand.ruleIndex = ruleIndex;

    // Add outputs to the graph and link to the build command
    std::vector<std::size_t> outIndices;
    for (std::string& out : outs) {
      const std::size_t outIndex = getPathIndex(out);
      outIndices.push_back(outIndex);
      nodeToCommand[outIndex] = commandIndex;
    }

    // Add inputs to the graph and add the edges to the graph
    for (std::string& in : ins) {
      const std::size_t inIndex = getPathIndex(in);
      for (const std::size_t outIndex : outIndices) {
        graph.addEdge(inIndex, outIndex);
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

  void operator()(RuleReader& r) {
    std::string_view name = r.name();
    const std::size_t ruleIndex = rules.size();
    const auto [ruleIt, inserted] = ruleLookup.emplace(name, ruleIndex);
    if (!inserted) {
      std::stringstream ss;
      ss << "Duplicate rule '" << name << "' found!" << '\0';
      throw std::runtime_error(ss.view().data());
    }

    Rule& rule = rules.emplace_back(ruleIt->first, parts.size()).first;
    for (VariableReader v : r.variables()) {
      const std::string_view key = v.name();
      if (!rule.add(key, v.value())) {
        throw std::runtime_error(std::format(
            "Unexpected variable '{}' in rule '{}' found!", key, name));
      }
    }

    parts.emplace_back(r.start(), r.position());
  }

  void operator()(DefaultReader& r) {
    std::vector<std::string> ins;
    for (const EvalString& path : r.paths()) {
      std::string result;
      evaluate(result, path, fileScope);
      ins.push_back(std::move(result));
    }

    const std::size_t partsIndex = parts.size();
    parts.emplace_back(r.start(), r.position());

    const std::size_t commandIndex = commands.size();
    BuildCommand& buildCommand = commands.emplace_back();
    buildCommand.partsIndex = partsIndex;
    buildCommand.ruleIndex = BuildContext::defaultIndex;

    const std::size_t outIndex = getDefault();
    nodeToCommand[outIndex] = commandIndex;
    for (std::string& in : ins) {
      graph.addEdge(getPathIndex(in), outIndex);
    }
  }

  void operator()(VariableReader& r) {
    std::string_view name = r.name();
    std::string result;
    evaluate(result, r.value(), fileScope);
    fileScope.set(name, std::move(result));
    parts.emplace_back(r.start(), r.position());
  }

  void operator()(IncludeReader& r) {
    const EvalString& pathEval = r.path();
    std::string path;
    evaluate(path, pathEval, fileScope);

    if (!std::filesystem::exists(path)) {
      throw std::runtime_error(std::format("Unable to find {}!", path));
    }
    std::stringstream ninjaCopy;
    std::ifstream ninja(path);
    ninjaCopy << ninja.rdbuf();
    fileContents.push_front(ninjaCopy.str());
    parse(path, fileContents.front());
  }

  void operator()(SubninjaReader&) {
    throw std::runtime_error("subninja not yet supported");
  }
};

void parseDepFile(const std::filesystem::path& ninjaDeps,
                  Graph& graph,
                  BuildContext& ctx) {
  // Later entries may override earlier entries so don't touch the graph until
  // we have parsed the whole file
  std::vector<std::string> paths;
  std::vector<std::vector<std::int32_t>> deps;
  for (const std::variant<PathRecordView, DepsRecordView>& record :
       DepsReader(ninjaDeps)) {
    switch (record.index()) {
      case 0: {
        const PathRecordView& view = std::get<PathRecordView>(record);
        if (static_cast<std::size_t>(view.index) >= paths.size()) {
          paths.resize(view.index + 1);
        }
        // Entries in `.ninja_deps` are already normalized when written
        paths[view.index] = view.path;
        break;
      }
      case 1: {
        const DepsRecordView& view = std::get<DepsRecordView>(record);
        if (static_cast<std::size_t>(view.outIndex) >= deps.size()) {
          deps.resize(view.outIndex + 1);
        }
        deps[view.outIndex].assign(view.deps.begin(), view.deps.end());
        break;
      }
    }
  }

  std::vector<std::size_t> lookup(paths.size());
  std::transform(paths.cbegin(), paths.cend(), lookup.begin(),
                 [&](const std::string_view path) {
                   return ctx.getPathIndexForNormalized(path);
                 });

  for (std::size_t outIndex = 0; outIndex < deps.size(); ++outIndex) {
    for (const std::int32_t inIndex : deps[outIndex]) {
      graph.addEdge(lookup[inIndex], lookup[outIndex]);
    }
  }
}

template <typename GET_HASH>
void parseLogFile(const std::filesystem::path& ninjaLog,
                  const BuildContext& ctx,
                  std::vector<bool>& isAffected,
                  GET_HASH&& get_hash,
                  bool explain) {
  std::ifstream deps(ninjaLog);

  // As there can be duplicate entries and subsequent entries take precedence
  // first record everything we care about and then update the graph
  const Graph& graph = ctx.graph;
  std::vector<bool> seen(graph.size());
  std::vector<bool> hashMismatch(graph.size());
  for (const LogEntry& entry : LogReader(deps)) {
    // Entries in `.ninja_log` are already normalized when written
    const std::optional<std::size_t> index =
        graph.findNormalizedPath(entry.output.string());
    if (!index) {
      // If we don't have the path then it was since removed from the ninja
      // build file
      continue;
    }

    seen[*index] = true;
    hashMismatch[*index] = entry.hash != get_hash(*index);
  }

  // Mark all build commands that are new or have been changed as required
  for (std::size_t index = 0; index < seen.size(); ++index) {
    const bool isBuildCommand = !graph.in(index).empty();
    if (isAffected[index] || !isBuildCommand) {
      continue;
    }

    // buil-in rules don't appear in the build log so skip them
    if (BuildContext::isBuiltInRule(
            ctx.commands[ctx.nodeToCommand[index]].ruleIndex)) {
      continue;
    }

    if (!seen[index]) {
      isAffected[index] = true;
      if (explain) {
        std::cerr << "Including '" << graph.path(index)
                  << "' as it was not found in '" << ninjaLog << "'"
                  << std::endl;
      }
    } else if (hashMismatch[index]) {
      isAffected[index] = true;
      if (explain) {
        std::cerr << "Including '" << graph.path(index)
                  << "' as the build command hash differs in '" << ninjaLog
                  << "'" << std::endl;
      }
    }
  }
}

// If `index` has not been seen (using `seen`) then call
// `markIfChildrenAffected` for all inputs to `index` and then set
// `isAffected[index]` if any child is affected. Return whether this
void markIfChildrenAffected(std::size_t index,
                            std::vector<bool>& seen,
                            std::vector<bool>& isAffected,
                            const BuildContext& ctx,
                            bool explain) {
  if (seen[index]) {
    return;
  }
  seen[index] = true;

  // Always process all our children so that `isAffected` is updated for them
  const Graph& graph = ctx.graph;
  const auto& inIndices = graph.in(index);
  for (const std::size_t in : inIndices) {
    markIfChildrenAffected(in, seen, isAffected, ctx, explain);
  }

  if (isAffected[index]) {
    return;
  }

  // Otherwise, find out if at least one of our children is affected and if
  // so, mark ourselves as affected
  const auto it = std::ranges::find_if(
      inIndices, [&](const std::size_t index) { return isAffected[index]; });
  if (it != inIndices.end()) {
    if (explain) {
      // Only mention user-defined rules since built-in rules are always kept
      if (!BuildContext::isBuiltInRule(
              ctx.commands[ctx.nodeToCommand[index]].ruleIndex)) {
        std::cerr << "Including '" << graph.path(index)
                  << "' as it has the affected input '" << graph.path(*it)
                  << "'" << std::endl;
      }
    }
    isAffected[index] = true;
  }
}

// If `index` has not been seen (using `seen`) then call
// `ifAffectedMarkAllChildren` for all outputs to `index` and then set
// `isAffected[index]` if any child is affected. Return whether this
void ifAffectedMarkAllChildren(std::size_t index,
                               std::vector<bool>& seen,
                               std::vector<bool>& isAffected,
                               std::vector<bool>& needsAllInputs,
                               const BuildContext& ctx,
                               bool explain) {
  if (seen[index]) {
    return;
  }
  seen[index] = true;

  for (const std::size_t out : ctx.graph.out(index)) {
    ifAffectedMarkAllChildren(out, seen, isAffected, needsAllInputs, ctx,
                              explain);
  }

  // Nothing to do if we have no children
  if (ctx.graph.in(index).empty()) {
    return;
  }

  if (!BuildContext::isBuiltInRule(
          ctx.commands[ctx.nodeToCommand[index]].ruleIndex)) {
    if (isAffected[index]) {
      needsAllInputs[index] = true;
      return;
    }
  }

  // If any build commands requiring us are marked as needing all inputs then
  // mark ourselves as affected and that we also need all our inputs.
  const auto it = std::ranges::find_if(
      ctx.graph.out(index),
      [&](const std::size_t index) { return needsAllInputs[index]; });
  if (it != ctx.graph.out(index).end()) {
    if (!isAffected[index]) {
      if (explain) {
        std::cerr << "Including '" << ctx.graph.path(index)
                  << "' as it is a required input for the affected output '"
                  << ctx.graph.path(*it) << "'" << std::endl;
      }
      isAffected[index] = true;
    }
    needsAllInputs[index] = true;
  }
}

}  // namespace

void TrimUtil::trim(std::ostream& output,
                    const std::filesystem::path& ninjaFile,
                    const std::string& ninjaFileContents,
                    std::istream& affected,
                    bool explain) {
  BuildContext ctx;

  // Parse the build file, this needs to be the first thing so we choose the
  // canonical paths in the same way that ninja does
  ctx.parse(ninjaFile, ninjaFileContents);

  Graph& graph = ctx.graph;

  const std::filesystem::path ninjaFileDir = [&] {
    std::filesystem::path dir(ninjaFile);
    dir.remove_filename();
    return dir;
  }();

  const std::filesystem::path builddir = [&] {
    std::string path;
    ctx.fileScope.appendValue(path, "builddir");
    return ninjaFileDir / path;
  }();

  // Add all dynamic dependencies from `.ninja_deps` to the graph
  if (const std::filesystem::path ninjaDeps = builddir / ".ninja_deps";
      std::filesystem::exists(ninjaDeps)) {
    parseDepFile(ninjaDeps, graph, ctx);
  }

  std::vector<bool> isAffected(graph.size(), false);

  // Look through all log entries and mark as required those build commands that
  // are either absent in the log (representing new commands that have never
  // been run) or those whose hash has changed.
  if (const std::filesystem::path ninjaLog = builddir / ".ninja_log";
      !std::filesystem::exists(ninjaLog)) {
    // If we don't have a `.ninja_log` file then either the user didn't have
    // it,which is an error, or our previous run did not include any build
    // commands.
    if (explain) {
      std::cerr << "Unable to find '" << ninjaLog
                << "', so including everything" << std::endl;
    }
    isAffected.assign(isAffected.size(), true);
  } else {
    parseLogFile(
        ninjaLog, ctx, isAffected,
        [&](const std::size_t index) {
          return ctx.commands[ctx.nodeToCommand[index]].hash;
        },
        explain);
  }

  // Mark all files in `affected` as required
  for (std::string line; std::getline(affected, line);) {
    // First try the raw input
    {
      const std::optional<std::size_t> index = graph.findPath(line);
      if (index.has_value()) {
        if (explain && !isAffected[*index]) {
          std::cerr << "Including '" << line
                    << "' as it was marked as affected by the user"
                    << std::endl;
        }
        isAffected[*index] = true;
        continue;
      }
    }

    // If that does not indicate a path, try the absolute path
    std::filesystem::path p(line);
    if (!p.is_absolute()) {
      const std::filesystem::path absolute =
          std::filesystem::absolute(ninjaFileDir / p);
      std::string absoluteStr = absolute.string();
      const std::optional<std::size_t> index = graph.findPath(absoluteStr);
      if (index.has_value()) {
        if (explain && !isAffected[*index]) {
          std::cerr << "Including '" << line
                    << "' as it was marked as affected by the user"
                    << std::endl;
        }
        isAffected[*index] = true;
        continue;
      }
    }

    // If neither indicates a path, then try the path relative to the ninja
    // file
    if (!p.is_relative()) {
      const std::filesystem::path relative = p.lexically_relative(ninjaFileDir);
      std::string relativeStr = relative.string();
      const std::optional<std::size_t> index = graph.findPath(relativeStr);
      if (index.has_value()) {
        if (explain && !isAffected[*index]) {
          std::cerr << "Including '" << line
                    << "' as it was marked as affected by the user"
                    << std::endl;
        }
        isAffected[*index] = true;
        continue;
      }
    }

    std::cerr << "'" << line << "' not found in input file" << std::endl;
  }

  std::vector<bool> seen(graph.size());

  // Mark all outputs that have an affected input as affected
  for (std::size_t index = 0; index < graph.size(); ++index) {
    markIfChildrenAffected(index, seen, isAffected, ctx, explain);
  }

  // Mark all inputs to affected outputs as affected (they technically
  // aren't affected but they are required to be built in order to
  // be inputs to affected outputs)
  seen.assign(seen.size(), false);
  std::vector<bool> needsAllInputs(graph.size(), false);
  for (std::size_t index = 0; index < graph.size(); ++index) {
    ifAffectedMarkAllChildren(index, seen, isAffected, needsAllInputs, ctx,
                              explain);
  }

  // Mark all affected `BuildCommands` as needing to print them out
  for (std::size_t index = 0; index < graph.size(); ++index) {
    if (isAffected[index]) {
      const std::size_t commandIndex = ctx.nodeToCommand[index];
      if (commandIndex != std::numeric_limits<std::size_t>::max()) {
        ctx.commands[commandIndex].resolution = BuildCommand::Print;
      }
    }
  }

  // Go through all build commands, keep a note of rules that are needed and
  // `phony` out the build edges that weren't affected.
  std::forward_list<std::string> phonyStorage;
  std::vector<bool> ruleReferenced(ctx.rules.size());
  for (const BuildCommand& command : ctx.commands) {
    if (command.resolution == BuildCommand::Print) {
      ruleReferenced[command.ruleIndex] = true;
    } else {
      assert(command.resolution == BuildCommand::Phony);
      const std::initializer_list<std::string_view> parts = {
          command.outStr,
          command.validationStr.empty() ? ": phony" : ": phony ",
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
      ctx.parts[command.partsIndex] = std::string_view{phony};
    }
  }

  // Remove all rules that weren't referenced
  for (std::size_t ruleIndex = 0; ruleIndex < ctx.rules.size(); ++ruleIndex) {
    if (!ruleReferenced[ruleIndex]) {
      ctx.parts[ctx.rules[ruleIndex].second] = "";
    }
  }

  std::copy(ctx.parts.begin(), ctx.parts.end(),
            std::ostream_iterator<std::string_view>(output));
}

}  // namespace trimja
