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

#ifndef TRIMJA_NINJA_PARSER
#define TRIMJA_NINJA_PARSER

#include "graph.h"

#include <ninja/eval_env.h>

#include <array>
#include <filesystem>
#include <forward_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace trimja {

namespace detail {

struct TransparentHash {
  using is_transparent = void;
  std::size_t operator()(const std::string& v) const {
    return std::hash<std::string_view>{}(v);
  }
  std::size_t operator()(std::string_view v) const {
    return std::hash<std::string_view>{}(v);
  }
};

}  // namespace detail

class BasicScope {
  std::unordered_map<std::string,
                     std::string,
                     detail::TransparentHash,
                     std::equal_to<>>
      m_variables;

 public:
  BasicScope() = default;

  std::string_view set(std::string_view key, std::string&& value);

  bool appendValue(std::string& output, std::string_view name) const;
};

struct Rule {
  inline static const std::array<std::string_view, 11> reserved = {
      "command",          "depfile", "dyndep", "description", "deps",
      "generator",        "pool",    "restat", "rspfile",     "rspfile_content",
      "msvc_deps_prefix",
  };

  std::string_view name;
  std::array<unsigned char, reserved.size() + 1>
      lookup;  // TODO: pack into uint64_t
  std::vector<EvalString> bindings;

  // The location of our entire build command inside `BuildContext::parts`
  std::size_t partsIndex = std::numeric_limits<std::size_t>::max();

  // Create a `Rule` having the specified `name`. The string pointed to by
  // `name` MUST live longer than this object.
  explicit Rule(std::string_view name);

  static std::size_t getLookupIndex(std::string_view varName);

  bool add(std::string_view varName, EvalString&& value);

  const EvalString* lookupVar(std::string_view varName) const;
};

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

  // Return whether `ruleIndex` is a built-in rule (i.e. `default` or `phony`)
  static bool isBuiltInRule(std::size_t ruleIndex);

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

  // Our list of rules.
  std::vector<Rule> rules;

  // Our rules keyed by name
  std::unordered_map<std::string_view,
                     std::size_t,
                     detail::TransparentHash,
                     std::equal_to<>>
      ruleLookup;

  // Our top-level variables
  BasicScope fileScope;

  // Our graph
  Graph graph;

  BuildContext();

  std::size_t getPathIndex(std::string& path);
  std::size_t getPathIndexForNormalized(std::string_view path);

  std::size_t getDefault();
};

struct ParserUtil {
  static void parse(BuildContext& ctx,
                    const std::filesystem::path& ninjaFile,
                    std::string_view ninjaFileContents);
};

}  // namespace trimja

#endif
