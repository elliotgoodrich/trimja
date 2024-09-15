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
#include "parser.h"

#include <ninja/util.h>

#include <cassert>
#include <fstream>
#include <iostream>
#include <numeric>

namespace trimja {

namespace {

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
  std::ranges::transform(paths, lookup.begin(), [&](std::string_view path) {
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
                    std::string_view ninjaFileContents,
                    std::istream& affected,
                    bool explain) {
  BuildContext ctx;

  // Parse the build file, this needs to be the first thing so we choose the
  // canonical paths in the same way that ninja does
  ParserUtil::parse(ctx, ninjaFile, ninjaFileContents);

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
      ctx.parts[command.partsIndex] = std::string_view{phony};
    }
  }

  // Remove all rules that weren't referenced
  for (std::size_t ruleIndex = 0; ruleIndex < ctx.rules.size(); ++ruleIndex) {
    if (!ruleReferenced[ruleIndex]) {
      ctx.parts[ctx.rules[ruleIndex].partsIndex] = "";
    }
  }

  std::copy(ctx.parts.begin(), ctx.parts.end(),
            std::ostream_iterator<std::string_view>(output));
}

}  // namespace trimja
