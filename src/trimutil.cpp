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
                  BuildContext& ctx) {
  std::vector<std::size_t> lookup;
  for (const std::variant<PathRecordView, DepsRecordView>& record :
       DepsReader(ninjaDeps)) {
    switch (record.index()) {
      case 0: {
        const PathRecordView& view = std::get<PathRecordView>(record);
        if (static_cast<std::size_t>(view.index) >= lookup.size()) {
          lookup.resize(view.index + 1);
        }
        // Entries in `.ninja_deps` are already normalized when written
        lookup[view.index] = ctx.getPathIndexForNormalized(view.path);
        break;
      }
      case 1: {
        const DepsRecordView& view = std::get<DepsRecordView>(record);
        for (const std::int32_t inIndex : view.deps) {
          graph.addEdge(lookup[inIndex], lookup[view.outIndex]);
        }
        break;
      }
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
    if (isBuildCommand && (!seen[index] || hashMismatch[index])) {
      requirements[index] = Requirement::InputsAndOutputs;
    }
  }
}

}  // namespace

void TrimUtil::trim(std::ostream& output,
                    const std::filesystem::path& ninjaFile,
                    std::string_view ninjaFileContents,
                    std::istream& affected) {
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
      return ctx.commands[ctx.nodeToCommand[index]].hash;
    });
  }

  // Mark all files in `affected` as required
  for (std::string line; std::getline(affected, line);) {
    // First try the raw input
    {
      const std::optional<std::size_t> index = graph.findPath(line);
      if (index) {
        requirements[*index] = Requirement::InputsAndOutputs;
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
      if (index) {
        requirements[*index] = Requirement::InputsAndOutputs;
        continue;
      }
    }

    // If neither indicates a path, then try the path relative to the ninja
    // file
    if (!p.is_relative()) {
      const std::filesystem::path relative = p.lexically_relative(ninjaFileDir);
      std::string relativeStr = relative.string();
      const std::optional<std::size_t> index = graph.findPath(relativeStr);
      if (index) {
        requirements[*index] = Requirement::InputsAndOutputs;
        continue;
      }
    }

    std::cerr << "'" << line << "' not found in input file" << std::endl;
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
        const std::size_t commandIndex = ctx.nodeToCommand[index];
        if (commandIndex != std::numeric_limits<std::size_t>::max()) {
          ctx.commands[commandIndex].resolution = BuildCommand::Print;
        }
        break;
      }
    }
  }

  // Go through all commands that need to be `phony`ed and do so
  std::forward_list<std::string> phonyStorage;
  for (const BuildCommand& command : ctx.commands) {
    if (command.resolution == BuildCommand::Phony) {
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

  std::copy(ctx.parts.begin(), ctx.parts.end(),
            std::ostream_iterator<std::string_view>(output));
}

}  // namespace trimja
