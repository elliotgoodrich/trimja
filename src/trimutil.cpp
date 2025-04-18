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
#include "cpuprofiler.h"
#include "depsreader.h"
#include "edgescope.h"
#include "evalstring.h"
#include "fixed_string.h"
#include "graph.h"
#include "logreader.h"
#include "manifestparser.h"
#include "murmur_hash.h"
#include "rule.h"

#include <ninja/util.h>
#include <rapidhash/rapidhash.h>
#include <boost/boost_unordered.hpp>

#include <cassert>
#include <forward_list>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

namespace trimja {

namespace {

// A vector of strings that are reused to avoid reallocations
class PathVector {
  std::vector<std::string> m_paths;
  std::size_t m_size;

 public:
  PathVector() : m_paths{}, m_size{0} {}

  std::string& operator[](std::size_t index) {
    assert(index < m_size);
    return m_paths[index];
  }

  std::string& emplace_back() {
    if (m_size < m_paths.size()) {
      return m_paths[m_size++];
    } else {
      // Give each path a reasonable size to avoid reallocations
      std::string& back = m_paths.emplace_back();
      back.reserve(1024);
      ++m_size;
      return back;
    }
  }

  void clear() {
    // Keep the objects around but just call `clear()` so that we
    // keep the memory around to be reused.
    std::for_each(m_paths.begin(), m_paths.begin() + m_size,
                  [](std::string& path) { path.clear(); });
    m_size = 0;
  }

  std::string* begin() { return m_paths.data(); }
  std::string* end() { return m_paths.data() + m_size; }

  const std::string* data() const { return m_paths.data(); }

  std::size_t size() const { return m_size; }
  bool empty() const { return m_size == 0; }
};

class NestedScope {
  std::vector<BasicScope> m_scopes;

 public:
  NestedScope() : m_scopes{1} {}

  void push() {
    BasicScope last = m_scopes.back();
    m_scopes.push_back(std::move(last));
  }

  [[nodiscard]] std::string pop() {
    // Take all variables defined in the latest scope and if their value differs
    // from the value in the previous scope then generate some Ninja variable
    // statements to set this variable back to the parent's value.
    BasicScope last = std::move(m_scopes.back());
    m_scopes.pop_back();

    std::string ninja;
    std::string previousValue;
    for (const auto& [name, value] : last) {
      previousValue.clear();
      m_scopes.back().appendValue(previousValue, name);
      if (value != previousValue) {
        ninja += name;
        if (previousValue.empty()) {
          ninja += " =";
        } else {
          ninja += " = ";
          ninja += previousValue;
        }
        ninja += '\n';
      }
    }

    return ninja;
  }

  void set(std::string_view key, std::string&& value) {
    m_scopes.back().set(key, std::move(value));
  }

  bool appendValue(std::string& output, std::string_view name) const {
    return m_scopes.back().appendValue(output, name);
  }
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
  gch::small_vector<std::size_t, 3> partsIndices;

  // The build command (+ rspfile_content) that gets hashed by ninja
  std::string hashTarget;

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

struct RuleCommand {
  // Our list of variables
  Rule variables;

  // The name of our rule
  std::string_view name;

  // The number of rules (including this one) that have this name. e.g. if this
  // is the first rule with this name then `instance` will be 1.
  std::size_t instance = 1;

  // The location of our entire rule inside `BuildContext::parts`
  gch::small_vector<std::size_t, 3> partsIndices;

  // An id of the file that defined this rule
  std::size_t fileId = std::numeric_limits<std::size_t>::max();

  RuleCommand(std::string_view name) : name{name} {}
};

}  // namespace

namespace detail {

class BuildContext {
 public:
  // The indexes of the built-in rules within `rules`
  static const std::size_t phonyIndex = 0;
  static const std::size_t defaultIndex = 1;

  // An optional storage for any generated strings or strings whose lifetime
  // needs extending. e.g. This is useful when processing `include` and
  // `subninja` and we need to extend the lifetime of the file contents until
  // all parsing has finished.  We use a `std::forward_list` so that we get
  // stable references to the contents.
  std::forward_list<std::string> stringStorage;

  // A place to hold numbers as strings that can be put into `parts` if we have
  // duplicate rules and need a suffix.
  std::vector<fixed_string> numbers;

  enum class PartType : std::int8_t {
    BuildEdge,
    Pool,
    Rule,
    Variable,
    Default,
  };

  // All parts of the input build file, possibly indexing into an element of
  // `stringStorage`.  Note that we may swap out build command sections with
  // phony commands.
  std::vector<std::string_view> parts;

  std::vector<PartType> partsType;

  // All build commands and default statements mentioned
  std::vector<BuildCommand> commands;

  // Map each output index to the index within `command`.  Use -1 for a
  // value that isn't an output to a build command (i.e. a source file)
  std::vector<std::size_t> nodeToCommand;

  // Our list of rules
  std::vector<RuleCommand> rules;

  struct RuleBits {
    // The index of the rule in `rules`
    std::size_t ruleIndex;

    // How many rules have this name
    std::size_t duplicates;

    explicit RuleBits(std::size_t ruleIndex)
        : ruleIndex{ruleIndex}, duplicates{1} {}
  };

  // Our rules keyed by name
  boost::unordered_flat_map<fixed_string,
                            RuleBits,
                            std::hash<trimja::fixed_string>>
      ruleLookup;

  // A stack of rules that have shadowed rules in their parent file.
  std::vector<std::vector<std::size_t>> shadowedRules;

  // When entering into subninja files we keep a stack of the file ids
  // that are generated from an incremental counter.
  std::size_t nextFileId = 0;
  std::vector<std::size_t> fileIds;

  // Our top-level variables
  NestedScope fileScope;

  // Our graph
  Graph graph;

  // Variables to be reused to avoid reallocations
  struct {
    PathVector outs;
    PathVector ins;
    PathVector orderOnlyDeps;
    std::vector<std::size_t> outIndices;
  } tmp;

  // Return whether `ruleIndex` is a built-in rule (i.e. `default` or `phony`)
  static bool isBuiltInRule(std::size_t ruleIndex) {
    static_assert(phonyIndex == 0);
    static_assert(defaultIndex == 1);
    return ruleIndex < 2;
  }

  template <typename RANGE>
  static void consume(RANGE&& range) {
    for ([[maybe_unused]] auto&& _ : range) {
    }
  }

  std::string_view to_string_view(std::size_t n) {
    for (std::size_t i = numbers.size(); i <= n; ++i) {
      numbers.emplace_back(std::to_string(i));
    }

    return numbers[n];
  }

  BuildContext() {
    fileIds.push_back(nextFileId++);

    rules.emplace_back(
        ruleLookup.try_emplace("phony", rules.size()).first->first);
    assert(rules[BuildContext::phonyIndex].name == "phony");
    rules.emplace_back(
        ruleLookup.try_emplace("default", rules.size()).first->first);
    assert(rules[BuildContext::defaultIndex].name == "default");
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
    consume(r.readVariables());
    parts.emplace_back(r.start(), r.bytesParsed());
    partsType.push_back(PartType::Pool);
  }

  void operator()(BuildReader& r) {
    PathVector& outs = tmp.outs;
    outs.clear();

    for (const EvalString& path : r.readOut()) {
      evaluate(outs.emplace_back(), path, fileScope);
    }
    if (outs.empty()) {
      throw std::runtime_error("Missing output paths in build command");
    }
    const std::size_t outSize = outs.size();
    for (const EvalString& path : r.readImplicitOut()) {
      evaluate(outs.emplace_back(), path, fileScope);
    }

    // Mark the outputs for later
    const std::string_view outStr(r.start(), r.bytesParsed());

    std::string_view ruleName = r.readName();

    const std::size_t ruleIndex = [&] {
      const auto ruleIt = ruleLookup.find(ruleName);
      if (ruleIt == ruleLookup.end()) {
        throw std::runtime_error("Unable to find " + std::string(ruleName) +
                                 " rule");
      }
      return ruleIt->second.ruleIndex;
    }();

    // Collect inputs
    PathVector& ins = tmp.ins;
    ins.clear();
    for (const EvalString& path : r.readIn()) {
      evaluate(ins.emplace_back(), path, fileScope);
    }
    const std::size_t inSize = ins.size();

    for (const EvalString& path : r.readImplicitIn()) {
      evaluate(ins.emplace_back(), path, fileScope);
    }

    PathVector& orderOnlyDeps = tmp.orderOnlyDeps;
    orderOnlyDeps.clear();
    for (const EvalString& path : r.readOrderOnlyDeps()) {
      evaluate(orderOnlyDeps.emplace_back(), path, fileScope);
    }

    // Collect validations but ignore what they are. If we include a build
    // command it will include the validation.  If that validation has a
    // required input then we include that, otherwise the validation is
    // `phony`ed out.
    const char* validationStart = r.position();
    consume(r.readValidations());
    const std::string_view validationStr{
        validationStart,
        static_cast<std::size_t>(r.position() - validationStart)};

    EdgeScope scope{fileScope, rules[ruleIndex].variables,
                    std::span{ins.data(), inSize},
                    std::span{outs.data(), outSize}};

    for (const auto& [name, value] : r.readVariables()) {
      std::string varValue;
      evaluate(varValue, value, scope);
      scope.set(name, std::move(varValue));
    }

    // Add the build command
    const std::size_t commandIndex = commands.size();
    BuildCommand& buildCommand = commands.emplace_back();

    // Always print `phony` rules since it saves us time generating an
    // identical `phony` rule later on.
    buildCommand.resolution =
        isBuiltInRule(ruleIndex) ? BuildCommand::Print : BuildCommand::Phony;

    const std::size_t partsIndex = parts.size();
    if (rules[ruleIndex].instance == 1) {
      parts.emplace_back(r.start(), r.bytesParsed());
      partsType.push_back(PartType::BuildEdge);
      buildCommand.partsIndices.push_back(partsIndex);
    } else {
      const char* endOfName = ruleName.data() + ruleName.size();
      parts.emplace_back(r.start(), endOfName - r.start());
      partsType.push_back(PartType::BuildEdge);
      buildCommand.partsIndices.push_back(partsIndex);

      parts.push_back(to_string_view(rules[ruleIndex].instance));
      partsType.push_back(PartType::BuildEdge);
      buildCommand.partsIndices.push_back(partsIndex + 1);

      parts.emplace_back(endOfName, r.bytesParsed() - (endOfName - r.start()));
      partsType.push_back(PartType::BuildEdge);
      buildCommand.partsIndices.push_back(partsIndex + 2);
    }
    // Check we aren't actually allocating
    assert(buildCommand.partsIndices.size() <=
           buildCommand.partsIndices.inline_capacity_v);

    buildCommand.validationStr = validationStr;
    buildCommand.outStr = outStr;
    buildCommand.ruleIndex = ruleIndex;

    // Add outputs to the graph and link to the build command
    std::vector<std::size_t>& outIndices = tmp.outIndices;
    outIndices.clear();
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

    // We only need to add an input to output edge and not a bidirectional
    // one for order-only dependencies. This is because we only include a
    // build edge if an input (implicit or not) is affected.
    for (std::string& orderOnlyDep : orderOnlyDeps) {
      const std::size_t inIndex = getPathIndex(orderOnlyDep);
      for (const std::size_t outIndex : outIndices) {
        graph.addOneWayEdge(inIndex, outIndex);
      }
    }

    scope.appendValue(buildCommand.hashTarget, "command");
    const std::size_t initialSize = buildCommand.hashTarget.size();
    scope.appendValue(buildCommand.hashTarget, "rspfile_content");

    // If `rspfile_content` is not empty we have to inject a separator
    if (buildCommand.hashTarget.size() != initialSize) {
      buildCommand.hashTarget.insert(initialSize, ";rspfile=");
    }
  }

  void operator()(RuleReader& r) {
    std::string_view name = r.name();
    const auto [ruleIt, isNew] = ruleLookup.try_emplace(name, rules.size());

    if (!isNew) {
      if (isBuiltInRule(ruleIt->second.ruleIndex)) {
        // We cannot shadow the built-in rules
        std::string msg;
        msg += "Cannot create a rule with the name '";
        msg += name;
        msg += "' as it is a built-in ninja rule!";
        throw std::runtime_error{msg};
      }

      RuleCommand& ruleCommand = rules[ruleIt->second.ruleIndex];
      if (ruleCommand.fileId == fileIds.back()) {
        // Throw an exception if we have a duplicate rule in the same file
        std::string msg;
        msg += "Duplicate rule '";
        msg += name;
        msg += "' found!";
        throw std::runtime_error{msg};
      }

      RuleBits& bits = ruleIt->second;
      if (!shadowedRules.empty()) {
        // The root ninja file may shadow existing rules added through
        // subninja, but we never need to unshadow these
        shadowedRules.back().push_back(bits.ruleIndex);
      }
      bits.ruleIndex = rules.size();
      ++bits.duplicates;
    }

    const char* endOfName = name.data() + name.size();
    const std::size_t bytesToEndOfName = endOfName - r.start();

    RuleCommand& rule = rules.emplace_back(name);
    rule.fileId = fileIds.back();
    rule.instance = ruleIt->second.duplicates;
    if (!isNew) {
      // If shadowed we need to add the rule suffix to the list of parts to
      // print
      const std::size_t partsIndex = parts.size();
      parts.emplace_back(r.start(), bytesToEndOfName);
      partsType.push_back(PartType::Rule);
      rule.partsIndices.push_back(partsIndex);

      parts.push_back(to_string_view(ruleIt->second.duplicates));
      partsType.push_back(PartType::Rule);
      rule.partsIndices.push_back(partsIndex + 1);
    }

    for (const auto& [key, value] : r.readVariables()) {
      if (!rule.variables.add(key, value)) {
        std::string msg;
        msg += "Unexpected variable '";
        msg += key;
        msg += "' in rule '";
        msg += name;
        msg += "' found!";
        throw std::runtime_error(msg);
      }
    }

    const std::size_t partsIndex = parts.size();
    if (!isNew) {
      // Include the rest of the variables if we're shadowed
      parts.emplace_back(endOfName, r.bytesParsed() - bytesToEndOfName);
      partsType.push_back(PartType::Rule);
      rule.partsIndices.push_back(partsIndex);
      assert(rule.partsIndices.size() == 3);
    } else {
      // If we're not shadowed then we can include the whole rule
      parts.emplace_back(r.start(), r.bytesParsed());
      partsType.push_back(PartType::Rule);
      rule.partsIndices.push_back(partsIndex);
      assert(rule.partsIndices.size() == 1);
    }
    // Check we aren't actually allocating
    assert(rule.partsIndices.size() <= rule.partsIndices.inline_capacity_v);
  }

  void operator()(DefaultReader& r) {
    PathVector& ins = tmp.ins;
    ins.clear();
    for (const EvalString& path : r.readPaths()) {
      evaluate(ins.emplace_back(), path, fileScope);
    }

    const std::size_t partsIndex = parts.size();
    parts.emplace_back(r.start(), r.bytesParsed());
    partsType.push_back(PartType::Default);

    const std::size_t commandIndex = commands.size();
    BuildCommand& buildCommand = commands.emplace_back();
    buildCommand.resolution = BuildCommand::Print;
    buildCommand.partsIndices.push_back(partsIndex);
    buildCommand.ruleIndex = BuildContext::defaultIndex;

    const std::size_t outIndex = getDefault();
    nodeToCommand[outIndex] = commandIndex;
    for (std::string& in : ins) {
      graph.addEdge(getPathIndex(in), outIndex);
    }
  }

  void operator()(const VariableReader& r) {
    std::string value;
    evaluate(value, r.value(), fileScope);
    fileScope.set(r.name(), std::move(value));
    parts.emplace_back(r.start(), r.bytesParsed());
    partsType.push_back(PartType::Variable);
  }

  void operator()(const IncludeReader& r) {
    const std::filesystem::path file = [&] {
      const EvalString& pathEval = r.path();
      std::string path;
      evaluate(path, pathEval, fileScope);
      return std::filesystem::path(r.parent()).remove_filename() / path;
    }();

    if (!std::filesystem::exists(file)) {
      std::string msg;
      msg += "Unable to find ";
      msg += file.string();
      msg += "!";
      throw std::runtime_error(msg);
    }
    std::stringstream ninjaCopy;
    std::ifstream ninja(file);
    ninjaCopy << ninja.rdbuf();
    stringStorage.push_front(ninjaCopy.str());
    parse(file, stringStorage.front());
  }

  void operator()(const SubninjaReader& r) {
    const std::filesystem::path file = [&] {
      const EvalString& pathEval = r.path();
      std::string path;
      evaluate(path, pathEval, fileScope);
      return std::filesystem::path{r.parent()}.remove_filename() / path;
    }();

    if (!std::filesystem::exists(file)) {
      std::string msg;
      msg += "Unable to find ";
      msg += file.string();
      msg += "!";
      throw std::runtime_error{msg};
    }

    fileScope.push();
    shadowedRules.emplace_back();

    std::stringstream ninjaCopy;
    std::ifstream ninja{file};
    ninjaCopy << ninja.rdbuf();
    stringStorage.push_front(ninjaCopy.str());

    fileIds.push_back(nextFileId++);
    parse(file, stringStorage.front());
    fileIds.pop_back();

    // Everything pushed back from popping a scope is a variable assignment
    stringStorage.push_front(fileScope.pop());
    parts.push_back(stringStorage.front());
    partsType.push_back(PartType::Variable);

    // For all the shadowed rules, set name to ruleIndex lookup back to the
    // shadowed index.  We have to grab the name and then find since
    // `unordered_flat_map` doesn't have iterator/reference stability by default
    for (const std::size_t shadowedRuleIndex : shadowedRules.back()) {
      const RuleCommand& shadowedRule = rules[shadowedRuleIndex];
      ruleLookup.find(shadowedRule.name)->second.ruleIndex = shadowedRuleIndex;
    }
    shadowedRules.pop_back();
  }
};

}  // namespace detail

namespace {

void parseDepFile(const std::filesystem::path& ninjaDeps,
                  Graph& graph,
                  detail::BuildContext& ctx) {
  // Later entries may override earlier entries so don't touch the graph until
  // we have parsed the whole file
  std::vector<std::string> paths;
  std::vector<std::vector<std::int32_t>> deps;
  std::ifstream depStream{ninjaDeps, std::ios_base::binary};
  try {
    for (const std::variant<PathRecordView, DepsRecordView>& record :
         DepsReader{depStream}) {
      std::visit(
          [&](auto&& view) {
            using T = std::decay_t<decltype(view)>;
            if constexpr (std::is_same<T, PathRecordView>()) {
              paths.resize(std::max<std::size_t>(paths.size(), view.index + 1));
              // Entries in `.ninja_deps` are already normalized when written
              paths[view.index] = view.path;
            } else {
              deps.resize(
                  std::max<std::size_t>(deps.size(), view.outIndex + 1));
              deps[view.outIndex].assign(view.deps.begin(), view.deps.end());
            }
          },
          record);
    }
  } catch (const std::exception& e) {
    std::string msg;
    msg += "Error processing ";
    msg += ninjaDeps.string();
    msg += ": ";
    msg += e.what();
    throw std::runtime_error{msg};
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

template <typename F>
void parseLogFile(const std::filesystem::path& ninjaLog,
                  const detail::BuildContext& ctx,
                  std::vector<bool>& isAffected,
                  F&& getBuildCommand,
                  bool explain) {
  std::ifstream deps(ninjaLog);

  // As there can be duplicate entries and subsequent entries take precedence
  // first record everything we care about and then update the graph
  const Graph& graph = ctx.graph;
  std::vector<bool> seen(graph.size());
  std::vector<bool> hashMismatch(graph.size());
  std::vector<std::optional<std::uint64_t>> cachedHashes(graph.size());
  for (const LogEntry& entry :
       LogReader{deps, LogEntry::Fields::out | LogEntry::Fields::hash}) {
    // Entries in `.ninja_log` are already normalized when written
    const std::optional<std::size_t> index =
        graph.findNormalizedPath(entry.out);
    if (!index) {
      // If we don't have the path then it was since removed from the ninja
      // build file
      continue;
    }

    seen[*index] = true;
    std::optional<std::uint64_t>& cachedHash = cachedHashes[*index];
    if (!cachedHash) {
      const std::string_view command = getBuildCommand(*index);
      switch (entry.hashType) {
        case HashType::murmur:
          cachedHash.emplace(murmur_hash::hash(command.data(), command.size()));
          break;
        case HashType::rapidhash:
          cachedHash.emplace(rapidhash(command.data(), command.size()));
          break;
        default:
          assert(false);  // TODO: `std::unreachable` in C++23
      }
    }
    hashMismatch[*index] = (entry.hash != *cachedHash);
  }

  // Mark all build commands that are new or have been changed as required
  for (std::size_t index = 0; index < seen.size(); ++index) {
    const bool isBuildCommand = !graph.in(index).empty();
    if (isAffected[index] || !isBuildCommand) {
      continue;
    }

    // buil-in rules don't appear in the build log so skip them
    if (detail::BuildContext::isBuiltInRule(
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
// NOLINTNEXTLINE(misc-no-recursion)
void markIfChildrenAffected(std::size_t index,
                            std::vector<bool>& seen,
                            std::vector<bool>& isAffected,
                            const detail::BuildContext& ctx,
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
  const auto it =
      std::find_if(inIndices.begin(), inIndices.end(),
                   [&](const std::size_t index) { return isAffected[index]; });
  if (it != inIndices.end()) {
    if (explain) {
      // Only mention user-defined rules since built-in rules are always kept
      if (!detail::BuildContext::isBuiltInRule(
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
// `ifRequiredRequireAllChildren` for all outputs to `index` and then set
// `isRequired[index]` if any child is required.
// NOLINTNEXTLINE(misc-no-recursion)
void ifRequiredRequireAllChildren(std::size_t index,
                                  std::vector<bool>& seen,
                                  std::vector<bool>& isRequired,
                                  std::vector<bool>& needsAllInputs,
                                  const detail::BuildContext& ctx,
                                  bool explain) {
  if (seen[index]) {
    return;
  }
  seen[index] = true;

  for (const std::size_t out : ctx.graph.out(index)) {
    ifRequiredRequireAllChildren(out, seen, isRequired, needsAllInputs, ctx,
                                 explain);
  }

  // Nothing to do if we have no children
  if (ctx.graph.in(index).empty()) {
    return;
  }

  if (!detail::BuildContext::isBuiltInRule(
          ctx.commands[ctx.nodeToCommand[index]].ruleIndex)) {
    if (isRequired[index]) {
      needsAllInputs[index] = true;
      return;
    }
  }

  // If any build commands requiring us are marked as needing all inputs then
  // mark ourselves as affected and that we also need all our inputs.
  const auto& outIndices = ctx.graph.out(index);
  const auto it = std::find_if(
      outIndices.begin(), outIndices.end(),
      [&](const std::size_t index) { return needsAllInputs[index]; });
  if (it != outIndices.end()) {
    if (!isRequired[index]) {
      if (explain) {
        std::cerr << "Including '" << ctx.graph.path(index)
                  << "' as it is a required input for the affected output '"
                  << ctx.graph.path(*it) << "'" << std::endl;
      }
      isRequired[index] = true;
    }
    needsAllInputs[index] = true;
  }
}
}  // namespace

TrimUtil::TrimUtil() : m_imp{nullptr} {}

TrimUtil::~TrimUtil() = default;

void TrimUtil::trim(std::ostream& output,
                    const std::filesystem::path& ninjaFile,
                    const std::string& ninjaFileContents,
                    std::istream& affected,
                    bool explain) {
  // Keep our state inside `m_imp` so that we defer cleanup until the destructor
  // of `TrimUtil`. This allows the calling code to skip all destructors when
  // calling `std::_Exit`.
  m_imp = std::make_unique<detail::BuildContext>();
  detail::BuildContext& ctx = *m_imp;

  // Parse the build file, this needs to be the first thing so we choose the
  // canonical paths in the same way that ninja does
  {
    const Timer t = CPUProfiler::start(".ninja parse");
    ctx.parse(ninjaFile, ninjaFileContents);
  }

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
    const Timer t = CPUProfiler::start(".ninja_deps parse");
    parseDepFile(ninjaDeps, graph, ctx);
  }

  std::vector<bool> isAffected(graph.size(), false);

  // Look through all log entries and mark as required those build commands that
  // are either absent in the log (representing new commands that have never
  // been run) or those whose hash has changed.
  if (const std::filesystem::path ninjaLog = builddir / ".ninja_log";
      !std::filesystem::exists(ninjaLog)) {
    // If we don't have a `.ninja_log` file then either the user didn't have
    // it, which is an error, or our previous run did not include any build
    // commands.
    if (explain) {
      std::cerr << "Unable to find '" << ninjaLog
                << "', so including everything" << std::endl;
    }
    isAffected.assign(isAffected.size(), true);
  } else {
    const Timer t = CPUProfiler::start(".ninja_log parse");
    parseLogFile(
        ninjaLog, ctx, isAffected,
        [&](const std::size_t index) -> std::string_view {
          return ctx.commands[ctx.nodeToCommand[index]].hashTarget;
        },
        explain);
  }

  // Mark all files in `affected` as required
  std::vector<std::filesystem::path> attempted;
  for (std::string line; std::getline(affected, line);) {
    if (line.empty()) {
      continue;
    }

    attempted.clear();

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
      std::error_code error;
      const std::filesystem::path& absolute =
          attempted.emplace_back(std::filesystem::absolute(p, error));
      if (!error) {
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
    }

    // If neither indicates a path, then try the path relative to the ninja
    // file
    if (!p.is_relative()) {
      std::error_code error;
      const std::filesystem::path& relative =
          attempted.emplace_back(std::filesystem::relative(p, error));
      if (!error) {
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
    }

    std::cerr << "'" << line << "' not found in input file";
    if (!attempted.empty()) {
      std::cerr << " (also tried ";
      const char* separator = "";
      for (const std::filesystem::path& path : attempted) {
        std::cerr << separator << "'" << path.string() << "'";
        separator = ", ";
      }
      std::cerr << ')';
    }
    std::cerr << std::endl;
  }

  std::vector<bool> seen(graph.size());

  // Mark all outputs that have an affected input as affected
  Timer trimTimer = CPUProfiler::start("trim time");
  for (std::size_t index = 0; index < graph.size(); ++index) {
    markIfChildrenAffected(index, seen, isAffected, ctx, explain);
  }

  // Keep the difference between build inputs that are affected (i.e. have
  // an input that is directly affected) or inputs that are required (i.e.
  // don't have an input that is directly affected but it itself is relied
  // on by an affected command)
  std::vector<bool> isRequired{isAffected};

  // Mark all inputs to affected outputs as required
  seen.assign(seen.size(), false);
  std::vector<bool> needsAllInputs(graph.size(), false);
  for (std::size_t index = 0; index < graph.size(); ++index) {
    ifRequiredRequireAllChildren(index, seen, isRequired, needsAllInputs, ctx,
                                 explain);
  }

  assert(ctx.parts.size() == ctx.partsType.size());
  std::vector<bool> immovable(ctx.parts.size());
  std::vector<bool> floatToTop(ctx.parts.size());
  for (std::size_t partIndex = 0; partIndex < ctx.partsType.size();
       ++partIndex) {
    switch (ctx.partsType[partIndex]) {
      case detail::BuildContext::PartType::Variable:
        immovable[partIndex] = true;
        break;
      case detail::BuildContext::PartType::Pool:
        [[fallthrough]];
      case detail::BuildContext::PartType::Rule:
        floatToTop[partIndex] = true;
        break;
      default:
        break;
    }
  }

  // Float all affected edges to the top so they are prioritized first
  // and Mark all required edges as needing to print them out
  for (std::size_t index = 0; index < graph.size(); ++index) {
    if (isRequired[index]) {
      const std::size_t commandIndex = ctx.nodeToCommand[index];
      if (commandIndex != std::numeric_limits<std::size_t>::max()) {
        BuildCommand& command = ctx.commands[commandIndex];
        command.resolution = BuildCommand::Print;

        if (isAffected[index]) {
          for (const std::size_t partsIndex : command.partsIndices) {
            floatToTop[partsIndex] = true;
          }
        }
      }
    } else {
      // All affected indices are required
      assert(!isAffected[index]);
    }
  }

  // Go through all build commands, keep a note of rules that are needed and
  // `phony` out the build edges that weren't required.
  std::forward_list<std::string> phonyStorage;
  std::vector<bool> ruleReferenced(ctx.rules.size());
  for (BuildCommand& command : ctx.commands) {
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

      // Clear all parts and replace them with 1 part that is the phony string
      assert(!command.partsIndices.empty());
      ctx.parts[command.partsIndices.front()] = std::string_view{phony};
      std::for_each(std::next(command.partsIndices.begin()),
                    command.partsIndices.end(),
                    [&](std::size_t index) { ctx.parts[index] = ""; });
      command.partsIndices.clear();
    }
  }

  // Remove all rules that weren't referenced and float the remaining to
  // the top so they remain above any build commands we are rearranging
  // imminently
  for (std::size_t ruleIndex = 0; ruleIndex < ctx.rules.size(); ++ruleIndex) {
    RuleCommand& rule = ctx.rules[ruleIndex];
    if (!ruleReferenced[ruleIndex]) {
      std::for_each(rule.partsIndices.begin(), rule.partsIndices.end(),
                    [&](const std::size_t index) { ctx.parts[index] = ""; });
    } else {
      std::for_each(rule.partsIndices.begin(), rule.partsIndices.end(),
                    [&](const std::size_t index) { floatToTop[index] = true; });
    }
  }

  // Float marked parts to the top of the file, making sure not to cross over
  // any parts marked as immovable
  auto start = ctx.parts.begin();
  while (start != ctx.parts.end()) {
    start = std::find_if(
        start, ctx.parts.end(), [&](const std::string_view& command) {
          const std::size_t partIndex = &command - ctx.parts.data();
          return !immovable[partIndex];
        });
    const auto end = std::find_if(
        start, ctx.parts.end(), [&](const std::string_view& command) {
          const std::size_t partIndex = &command - ctx.parts.data();
          return immovable[partIndex];
        });
    std::stable_partition(start, end, [&](const std::string_view& command) {
      const std::size_t partIndex = &command - ctx.parts.data();
      return floatToTop[partIndex];
    });
    start = end;
  }

  trimTimer.stop();

  const Timer writeTimer = CPUProfiler::start("output time");
  std::copy(ctx.parts.begin(), ctx.parts.end(),
            std::ostream_iterator<std::string_view>(output));
}

}  // namespace trimja
