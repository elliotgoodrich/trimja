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
#include "indexinto.h"
#include "logreader.h"
#include "manifestparser.h"
#include "murmur_hash.h"
#include "nestedscope.h"
#include "rulevariables.h"
#include "stringstack.h"

#include <ninja/util.h>
#include <rapidhash/rapidhash.h>
#include <boost/boost_unordered.hpp>

#include <cassert>
#include <fstream>
#include <iostream>
#include <list>
#include <memory_resource>
#include <numeric>
#include <sstream>

namespace trimja {

// NOLINTBEGIN(performance-avoid-endl)

namespace detail {

/**
 * @class ManagedResources
 * @brief A class to create long lived objects that will get automatically
 * cleaned up.
 * @private
 */
class ManagedResources {
  std::pmr::monotonic_buffer_resource m_buffer;

 public:
  /**
   * @brief Default constructor for ManagedResources.
   */
  ManagedResources() = default;

  /**
   * @brief Create a copy of a string in managed storage.
   * @param str The string to copy.
   * @return A string_view pointing to the copied string whose lifetime is as
   * long as this ManagedResources.
   */
  std::string_view copyString(std::string_view str) {
    char* out = static_cast<char*>(m_buffer.allocate(str.size()));
    std::copy(str.cbegin(), str.cend(), out);
    return std::string_view{out, str.size()};
  }

  /**
   * @brief Create an empty string in managed storage.
   * @return A std::pmr::string whose lifetime is as long as this
   * ManagedResources.
   */
  std::pmr::string& createManagedString() {
    std::pmr::polymorphic_allocator alloc{&m_buffer};
    return *alloc.new_object<std::pmr::string>();
  }

  /**
   * @brief Create an empty stringstream in managed storage.
   * @return A std::stringstream whose lifetime is as long as this
   * ManagedResources.
   */
  std::basic_stringstream<char,
                          std::char_traits<char>,
                          std::pmr::polymorphic_allocator<char>>&
  createManagedSStream() {
    std::pmr::polymorphic_allocator alloc{&m_buffer};
    return *alloc.new_object<std::basic_stringstream<
        char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>>(
        std::pmr::string{}, std::ios_base::in | std::ios_base::out);
  }
};

/**
 * @class OutputText
 * @brief A class to manage the final output of trimming the ninja file.
 * @private
 */
class OutputText {
  std::pmr::monotonic_buffer_resource m_buffer;

 public:
  enum class PartType : std::int8_t {
    BuildEdge,
    Pool,
    Rule,
    Variable,
    Default,
  };

  /**
   * @brief Whether a part of the output text can be moved.
   */
  enum class Movable : std::int8_t {
    No,   ///< This piece cannot be moved and acts a sequence point where no
          ///< other pieces can be moved before/after it.
    Yes,  ///< This piece can be moved, but not before/after pieces that are not
          ///< movable.
  };

  /**
   * @brief Whether a part of the output text should be moved as high as
   * possible.
   */
  enum class FloatToTop : std::int8_t {
    No,   ///< Do not move this part.
    Yes,  ///< Move this part up as high as possible, but not crossing paths
          ///< with a `Moveable::No` piece.
  };

  // All parts of the input build file.  Note that we may swap out build command
  // sections with phony commands.
  std::pmr::list<std::tuple<std::string_view, Movable, FloatToTop>> parts;

  using iterator = decltype(parts)::iterator;

  OutputText() : m_buffer{}, parts{&m_buffer} {}

  /**
   * @brief Add the text with the specified type to the end.
   * @param text The text to insert
   * @param type The type of the text inserted
   * @pre text must point to a string that outlives this object
   * @return An Index representing the newly inserted element
   */
  iterator emplace_back(std::string_view text, PartType type) {
    const auto [movable, floatToTop] = [&] {
      switch (type) {
        case OutputText::PartType::Variable:
          return std::make_pair(Movable::No, FloatToTop::No);
        case OutputText::PartType::Pool:
          [[fallthrough]];
        case OutputText::PartType::Rule:
          return std::make_pair(Movable::Yes, FloatToTop::Yes);
        default:
          return std::make_pair(Movable::Yes, FloatToTop::No);
      }
    }();
    return parts.emplace(parts.end(), text, movable, floatToTop);
  }

  /**
   * @brief Add the text with the specified type to the end.
   * @param data The start of the string
   * @param size The size of the string
   * @param type The type of the text inserted
   * @pre data must point to a string that outlives this object
   * @return An Index representing the newly inserted element
   */
  iterator emplace_back(const char* data, std::size_t size, PartType type) {
    return emplace_back(std::string_view{data, size}, type);
  }

  /**
   * @brief Return the number of elements
   * @return The number of elements.
   */
  std::size_t size() const { return parts.size(); }

  /**
   * @brief Rearrange the text parts.
   *
   * Rearrange the text parts so that all parts marked as `FloatToTop::Yes` come
   * before those marked `FloatToTop::No`, but do not move any parts before or
   * after those marked `Movable::No`.  The relative posisition between parts
   * with the same value for `FloatToTop` is unchanged.
   */
  void rearrange() {
    auto start = parts.begin();
    while (start != parts.end()) {
      start = std::find_if(start, parts.end(), [&](const auto& part) {
        return std::get<Movable>(part) == Movable::Yes;
      });
      const auto seqPoint =
          std::find_if(start, parts.end(), [&](const auto& part) {
            return std::get<Movable>(part) == Movable::No;
          });
      std::stable_partition(start, seqPoint, [&](const auto& part) {
        return std::get<FloatToTop>(part) == FloatToTop::Yes;
      });
      start = seqPoint;
    }
  }
};

/**
 * @class RuleTextParts
 * @brief A class to manage the parts of a ninja rule inside OutputText
 * @private
 */
class RuleTextParts {
  OutputText::iterator m_start;
  OutputText::iterator m_name;
  OutputText::iterator m_rest;

 public:
  /**
   * @brief Construct a RuleTextParts object.
   * @param start The index of the start of the rule, up to the start of the
   * rule name
   * @param name The index of the name of the rule
   * @param rest The index of the rest of the rule after the name, including all
   * variables.
   */
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  RuleTextParts(OutputText::iterator start,
                OutputText::iterator name,
                OutputText::iterator rest)
      : m_start{start}, m_name{name}, m_rest{rest} {}

  /**
   * @brief Get the name of this rule.
   * @return The name of this rule.
   */
  std::string_view getName() const {
    return std::get<std::string_view>(*m_name);
  }

  /**
   * @brief Set the name of this rule.
   * @param newName The new name of the rule.
   * @pre newName must point to a string that outlives output.
   */
  void setName(std::string_view newName) {
    std::get<std::string_view>(*m_name) = newName;
  }

  /**
   * @brief Get an iterator to the beginning of the rule parts.
   */
  OutputText::iterator begin() const { return m_start; }

  /**
   * @brief Get an iterator to the end of the rule parts.
   */
  OutputText::iterator end() const { return std::next(m_rest); }
};

/**
 * @brief Parse a RuleReader into its text parts and variables.
 * @param r The RuleReader to parse.
 * @param output The output text to insert the rule parts into.
 * @pre r has not had any non-const methods called on it.
 * @post r would have been completely read.
 * @return A pair of the RuleTextParts and RuleVariables parsed.
 */
std::pair<RuleTextParts, RuleVariables> parseRule(RuleReader& r,
                                                  OutputText& output) {
  const std::string_view name = r.name();

  RuleVariables variables;
  for (const auto& [key, value] : r.readVariables()) {
    if (!variables.add(key, value)) {
      std::string msg;
      msg += "Unexpected variable '";
      msg += key;
      msg += "' in rule '";
      msg += name;
      msg += "' found!";
      throw std::runtime_error{msg};
    }
  }

  const char* endOfName = name.data() + name.size();
  const auto startIt = output.emplace_back(r.start(), name.data() - r.start(),
                                           OutputText::PartType::Rule);
  const auto nameIt = output.emplace_back(name, OutputText::PartType::Rule);
  const auto restIt =
      output.emplace_back(name.data() + name.size(), r.position() - endOfName,
                          OutputText::PartType::Rule);

  return std::pair<RuleTextParts, RuleVariables>{
      std::piecewise_construct, std::forward_as_tuple(startIt, nameIt, restIt),
      std::forward_as_tuple(std::move(variables))};
}

/**
 * @class Rule
 * @brief A class to manage everything about a ninja rule
 * @private
 */
struct Rule {
  // The name of this rule.
  std::string_view name;

  // Our list of variables
  RuleVariables variables;

  // An optional RuleTextParts to contain our output into OutputText.
  // This is only nullopt when the rule is a built-in rule.
  std::optional<RuleTextParts> parts;

  // The number of rules (including this one) that have this name. e.g. if
  // this is the first rule with this name then `instance` will be 1.
  std::size_t instance = 1;

  // An id of the file that defined this rule or `nullopt` for built-in rules.
  std::optional<std::size_t> fileId;

  /**
   * @brief Construct a Rule object with a given name
   * @param name The name of the rule
   * @pre The lifetime of name exceeds the lifetime of this object
   */
  Rule(std::string_view name) : name{name} {}

  /**
   * @brief Get an iterator to the beginning of the rule parts.
   */
  OutputText::iterator begin() const {
    return parts ? parts->begin() : OutputText::iterator{};
  }

  /**
   * @brief Get an iterator to the end of the rule parts.
   */
  OutputText::iterator end() const {
    return parts ? parts->end() : OutputText::iterator{};
  }
};

class BuildContext {
 public:
  struct RuleTag {};
  using RuleIndex = IndexInto<BuildContext, RuleTag>;
  struct BuildCommandTag {};
  using BuildCommandIndex = IndexInto<BuildContext, BuildCommandTag>;
  struct NodeTag {};
  using Node = IndexInto<BuildContext, NodeTag, Graph::Node>;

  struct BuildCommand {
    enum Resolution : std::int8_t {
      // Print the entire build command
      Print,

      // Create a phony command for all the (implicit) outputs
      Phony,
    };

    Resolution resolution;

    // The location of our entire build command inside `BuildContext::parts`
    gch::small_vector<OutputText::iterator, 3> partIterators;

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
    RuleIndex ruleIndex;

    BuildCommand(Resolution resolution, RuleIndex ruleIndex)
        : resolution{resolution}, ruleIndex{ruleIndex} {}
  };

  // The indexes of the built-in rules within `rules`
  inline static const std::size_t phonyIndex = 0;
  inline static const std::size_t defaultIndex = 1;

  // A storage for generated strings or strings whose lifetime needs extending.
  // e.g. This is useful when processing `include` and `subninja` and we need to
  // extend the lifetime of the file contents until all parsing has finished.
  ManagedResources& storage;

  // A place to hold numbers as strings that can be put into `parts` if we have
  // duplicate rules and need a suffix.
  std::vector<fixed_string> numbers;

  OutputText output;

  // All build commands and default statements mentioned
  std::vector<BuildCommand> commands;

  // Map each output index to the index within `command`.  Use `nullopt` for
  // values that aren't an output to a build command (i.e. a source file)
  // TODO: Move back to `tiny::optional<BuildCommandIndex, sentinel>`, which is
  // failing to compile in my attempts.
  std::vector<std::optional<BuildCommandIndex>> nodeToCommand;

  std::vector<bool> isAffected;
  std::vector<bool> isRequired;

  // Our list of rules
  std::vector<Rule> rules;

  struct RuleBits {
    // The index of the rule in `rules`
    RuleIndex ruleIndex;

    // How many rules have this name
    std::size_t duplicates;

    explicit RuleBits(RuleIndex ruleIndex)
        : ruleIndex{ruleIndex}, duplicates{1} {}
  };

  // Our rules keyed by name
  boost::unordered_flat_map<fixed_string,
                            RuleBits,
                            std::hash<trimja::fixed_string>>
      ruleLookup;

  // A stack of rules that have shadowed rules in their parent file.
  std::vector<std::vector<RuleIndex>> shadowedRules;

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
    StringStack outs;
    StringStack ins;
    StringStack orderOnlyDeps;
    std::vector<Node> outNodes;
  } tmp;

  // Return whether `ruleIndex` is a built-in rule (i.e. `default` or `phony`)
  static bool isBuiltInRule(RuleIndex ruleIndex) {
    static_assert(phonyIndex == 0);
    static_assert(defaultIndex == 1);
    return ruleIndex < 2;
  }

  std::string_view popScopeAndRevertVariables() {
    BasicScope top = fileScope.pop();
    std::pmr::string& result = storage.createManagedString();
    for (const auto& [name, value] : top.revert(fileScope)) {
      result += name;
      if (value.empty()) {
        result += " =\n";
      } else {
        result += " = ";
        result += value;
        result += '\n';
      }
    }
    return result;
  }

  std::string_view to_string_view(std::size_t n) {
    for (std::size_t i = numbers.size(); i <= n; ++i) {
      numbers.emplace_back(std::to_string(i));
    }

    return numbers[n];
  }

  Node augment(const Graph::Node& node) const { return Node{node, this}; }

  BuildContext(ManagedResources& storage) : storage{storage} {
    fileIds.push_back(nextFileId++);

    rules.emplace_back(
        ruleLookup.try_emplace("phony", RuleIndex{rules.size(), this})
            .first->first);
    assert(rules[BuildContext::phonyIndex].name == "phony");
    rules.emplace_back(
        ruleLookup.try_emplace("default", RuleIndex{rules.size(), this})
            .first->first);
    assert(rules[BuildContext::defaultIndex].name == "default");
  }

  // Delete move/copy operations since we need pointer stability to `this` for
  // the `IndexInto` elements
  BuildContext(BuildContext&&) = delete;
  BuildContext(const BuildContext&) = delete;
  BuildContext& operator=(BuildContext&&) = delete;
  BuildContext& operator=(const BuildContext&) = delete;

  Node getPathNode(std::string&& path) {
    const Graph::Node node = graph.addPath(std::move(path));
    if (node >= nodeToCommand.size()) {
      nodeToCommand.resize(node + 1);
      isAffected.resize(node + 1);
      isRequired.resize(node + 1);
    }
    return augment(node);
  }

  Node getPathNodeForNormalized(std::string_view path) {
    const Graph::Node node = graph.addNormalizedPath(path);
    if (node >= nodeToCommand.size()) {
      nodeToCommand.resize(node + 1);
      isAffected.resize(node + 1);
      isRequired.resize(node + 1);
    }
    return augment(node);
  }

  Node getDefault() {
    const Graph::Node node = graph.addDefault();
    if (node >= nodeToCommand.size()) {
      nodeToCommand.resize(node + 1);
      isAffected.resize(node + 1);
      isRequired.resize(node + 1);
    }
    return augment(node);
  }

  void parse(const std::filesystem::path& ninjaFile,
             std::string_view ninjaFileContents) {
    for (auto&& part : ManifestReader{ninjaFile, ninjaFileContents}) {
      std::visit(*this, part);
    }
  }

  void operator()(PoolReader& r) {
    r.skip();
    output.emplace_back(r.start(), r.bytesParsed(), OutputText::PartType::Pool);
  }

  void operator()(BuildReader& r) {
    StringStack& outs = tmp.outs;
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

    const RuleIndex ruleIndex = [&] {
      const auto ruleIt = ruleLookup.find(ruleName);
      if (ruleIt == ruleLookup.end()) {
        throw std::runtime_error("Unable to find " + std::string(ruleName) +
                                 " rule");
      }
      return ruleIt->second.ruleIndex;
    }();

    // Collect inputs
    StringStack& ins = tmp.ins;
    ins.clear();
    for (const EvalString& path : r.readIn()) {
      evaluate(ins.emplace_back(), path, fileScope);
    }
    const std::size_t inSize = ins.size();

    for (const EvalString& path : r.readImplicitIn()) {
      evaluate(ins.emplace_back(), path, fileScope);
    }

    StringStack& orderOnlyDeps = tmp.orderOnlyDeps;
    orderOnlyDeps.clear();
    for (const EvalString& path : r.readOrderOnlyDeps()) {
      evaluate(orderOnlyDeps.emplace_back(), path, fileScope);
    }

    // Collect validations but ignore what they are. If we include a build
    // command it will include the validation.  If that validation has a
    // required input then we include that, otherwise the validation is
    // `phony`ed out.
    const char* validationStart = r.position();
    r.readValidations().skip();
    const std::string_view validationStr{
        validationStart,
        static_cast<std::size_t>(r.position() - validationStart)};

    EdgeScope scope{fileScope, rules[ruleIndex].variables,
                    std::span{ins.data(), inSize},
                    std::span{outs.data(), outSize}};

    for (const auto& [name, value] : r.readVariables()) {
      scope.set(name, evaluate(value, scope));
    }

    // Add the build command
    const BuildCommandIndex commandIndex{commands.size(), this};
    BuildCommand& buildCommand = commands.emplace_back(
        // Always print `phony` rules since it saves us time generating an
        // identical `phony` rule later on.
        isBuiltInRule(ruleIndex) ? BuildCommand::Print : BuildCommand::Phony,
        ruleIndex);

    if (rules[ruleIndex].instance == 1) {
      buildCommand.partIterators.push_back(output.emplace_back(
          r.start(), r.bytesParsed(), OutputText::PartType::BuildEdge));
    } else {
      const char* endOfName = ruleName.data() + ruleName.size();
      buildCommand.partIterators.push_back(output.emplace_back(
          r.start(), endOfName - r.start(), OutputText::PartType::BuildEdge));

      buildCommand.partIterators.push_back(
          output.emplace_back(to_string_view(rules[ruleIndex].instance),
                              OutputText::PartType::BuildEdge));

      buildCommand.partIterators.push_back(output.emplace_back(
          endOfName, r.bytesParsed() - (endOfName - r.start()),
          OutputText::PartType::BuildEdge));
    }
    // Check we aren't actually allocating
    assert(buildCommand.partIterators.size() <=
           buildCommand.partIterators.inline_capacity_v);

    buildCommand.validationStr = validationStr;
    buildCommand.outStr = outStr;

    // Add outputs to the graph and link to the build command
    std::vector<Node>& outNodes = tmp.outNodes;
    outNodes.clear();
    for (std::string& out : outs) {
      const Node outNode = getPathNode(std::move(out));
      outNodes.push_back(outNode);
      nodeToCommand[outNode] = commandIndex;
    }

    // Add inputs to the graph and add the edges to the graph
    for (std::string& in : ins) {
      const Node inNode = getPathNode(std::move(in));
      for (const Node& outNode : outNodes) {
        graph.addEdge(inNode, outNode);
      }
    }

    // We only need to add an input to output edge and not a bidirectional
    // one for order-only dependencies. This is because we only include a
    // build edge if an input (implicit or not) is affected.
    for (std::string& orderOnlyDep : orderOnlyDeps) {
      const Node inNode = getPathNode(std::move(orderOnlyDep));
      for (const Node& outNode : outNodes) {
        graph.addOneWayEdge(inNode, outNode);
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
    auto [ruleParts, ruleVariables] = parseRule(r, output);
    const std::string_view name = ruleParts.getName();
    const RuleIndex ruleIndex{rules.size(), this};
    const auto [ruleIt, isNew] = ruleLookup.try_emplace(name, ruleIndex);

    if (!isNew) {
      if (isBuiltInRule(ruleIt->second.ruleIndex)) {
        // We cannot shadow the built-in rules
        std::string msg;
        msg += "Cannot create a rule with the name '";
        msg += name;
        msg += "' as it is a built-in ninja rule!";
        throw std::runtime_error{msg};
      }

      const Rule& rule = rules[ruleIt->second.ruleIndex];
      if (rule.fileId == fileIds.back()) {
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
      bits.ruleIndex = ruleIndex;
      ++bits.duplicates;
    }

    Rule& rule = rules.emplace_back(name);
    rule.parts = ruleParts;
    rule.fileId = fileIds.back();
    rule.instance = ruleIt->second.duplicates;
    rule.variables = std::move(ruleVariables);
    if (!isNew) {
      // If shadowed we need to add the rule suffix to the list of parts to
      // print
      std::pmr::string& newName = storage.createManagedString();
      newName = name;
      newName += std::to_string(ruleIt->second.duplicates);
      rule.parts->setName(newName);
    }
  }

  void operator()(DefaultReader& r) {
    StringStack& ins = tmp.ins;
    ins.clear();
    for (const EvalString& path : r.readPaths()) {
      evaluate(ins.emplace_back(), path, fileScope);
    }

    const auto partIt = output.emplace_back(r.start(), r.bytesParsed(),
                                            OutputText::PartType::Default);

    const BuildCommandIndex commandIndex{commands.size(), this};
    BuildCommand& buildCommand = commands.emplace_back(
        BuildCommand::Print, RuleIndex{BuildContext::defaultIndex, this});
    buildCommand.partIterators.push_back(partIt);

    const Node outNode = getDefault();
    nodeToCommand[outNode] = commandIndex;
    for (std::string& in : ins) {
      graph.addEdge(getPathNode(std::move(in)), outNode);
    }
  }

  void operator()(const VariableReader& r) {
    fileScope.set(r.name(), evaluate(r.value(), fileScope));
    output.emplace_back(r.start(), r.bytesParsed(),
                        OutputText::PartType::Variable);
  }

  void operator()(const IncludeReader& r) {
    const std::filesystem::path file =
        std::filesystem::path{r.parent()}.remove_filename() /
        evaluate(r.path(), fileScope);

    if (!std::filesystem::exists(file)) {
      std::string msg;
      msg += "Unable to find ";
      msg += file.string();
      msg += "!";
      throw std::runtime_error(msg);
    }

    auto& stream = storage.createManagedSStream();
    const std::ifstream ninja{file};
    stream << ninja.rdbuf();
    stream << '\0';  // ensure our `string_view` is null-terminated
    parse(file, stream.view());
  }

  void operator()(const SubninjaReader& r) {
    const std::filesystem::path file =
        std::filesystem::path{r.parent()}.remove_filename() /
        evaluate(r.path(), fileScope);

    if (!std::filesystem::exists(file)) {
      std::string msg;
      msg += "Unable to find ";
      msg += file.string();
      msg += "!";
      throw std::runtime_error{msg};
    }

    fileScope.push();
    shadowedRules.emplace_back();

    auto& stream = storage.createManagedSStream();
    const std::ifstream ninja{file};
    stream << ninja.rdbuf();
    stream << '\0';  // ensure our `string_view` is null-terminated

    fileIds.push_back(nextFileId++);
    parse(file, stream.view());
    fileIds.pop_back();

    // Everything pushed back from popping a scope is a variable assignment
    output.emplace_back(popScopeAndRevertVariables(),
                        OutputText::PartType::Variable);

    // For all the shadowed rules, set name to ruleIndex lookup back to the
    // shadowed index.  We have to grab the name and then find since
    // `unordered_flat_map` doesn't have iterator/reference stability by default
    for (const RuleIndex shadowedRuleIndex : shadowedRules.back()) {
      const Rule& shadowedRule = rules[shadowedRuleIndex];
      ruleLookup.find(shadowedRule.name)->second.ruleIndex = shadowedRuleIndex;
    }
    shadowedRules.pop_back();
  }

  void parseDepFile(const std::filesystem::path& ninjaDeps) {
    // Later entries may override earlier entries so don't touch the graph until
    // we have parsed the whole file
    std::vector<std::string> paths;  // NOLINTLINE(misc-const-correctness)
    std::vector<std::vector<std::int32_t>> deps;
    std::ifstream depStream{ninjaDeps, std::ios_base::binary};
    try {
      for (const std::variant<PathRecordView, DepsRecordView>& record :
           DepsReader{depStream}) {
        std::visit(
            [&](auto&& view) {
              using T = std::decay_t<decltype(view)>;
              if constexpr (std::is_same<T, PathRecordView>()) {
                paths.resize(std::max(
                    paths.size(), static_cast<std::size_t>(view.index) + 1));
                // Entries in `.ninja_deps` are already normalized when written
                paths[view.index] = view.path;
              } else {
                deps.resize(std::max(
                    deps.size(), static_cast<std::size_t>(view.outIndex) + 1));
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

    std::vector<Node> lookup(paths.size());
    std::transform(paths.cbegin(), paths.cend(), lookup.begin(),
                   [&](const std::string_view path) {
                     return getPathNodeForNormalized(path);
                   });

    for (std::size_t outIndex = 0; outIndex < deps.size(); ++outIndex) {
      for (const std::int32_t inIndex : deps[outIndex]) {
        graph.addEdge(lookup[inIndex], lookup[outIndex]);
      }
    }
  }

  template <typename F>
  void parseLogFile(const std::filesystem::path& ninjaLog,
                    F&& getBuildCommand,
                    bool explain) {
    std::ifstream deps(ninjaLog);

    // As there can be duplicate entries and subsequent entries take precedence
    // first record everything we care about and then update the graph
    std::vector<bool> seen(graph.size());
    std::vector<bool> hashMismatch(graph.size());
    std::vector<std::optional<std::uint64_t>> cachedHashes(graph.size());
    for (const LogEntry& entry :
         LogReader{deps, LogEntry::Fields::out | LogEntry::Fields::hash}) {
      // Entries in `.ninja_log` are already normalized when written
      const std::optional<Graph::Node> node =
          graph.findNormalizedPath(entry.out);
      if (!node) {
        // If we don't have the path then it was since removed from the ninja
        // build file
        continue;
      }

      seen[*node] = true;
      std::optional<std::uint64_t>& cachedHash = cachedHashes[*node];
      if (!cachedHash) {
        const std::string_view command = getBuildCommand(augment(*node));
        switch (entry.hashType) {
          case HashType::murmur:
            cachedHash.emplace(
                murmur_hash::hash(command.data(), command.size()));
            break;
          case HashType::rapidhash:
            cachedHash.emplace(rapidhash(command.data(), command.size()));
            break;
          default:
            assert(false);  // TODO: `std::unreachable` in C++23
        }
      }
      hashMismatch[*node] = (entry.hash != *cachedHash);
    }

    // Mark all build commands that are new or have been changed as required
    for (const Node& node : nodes()) {
      const bool isBuildCommand = !graph.in(node).empty();
      if (isAffected[node] || !isBuildCommand) {
        continue;
      }

      // If we get to this place then we have children, therefore
      // we must have been built by a command
      const std::optional<BuildCommandIndex>& commandIndex =
          nodeToCommand[node];
      assert(commandIndex.has_value());
      // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
      const std::size_t idx = *commandIndex;

      // built-in rules don't appear in the build log so skip them
      if (isBuiltInRule(commands[idx].ruleIndex)) {
        continue;
      }

      if (!seen[node]) {
        isAffected[node] = true;
        if (explain) {
          std::cerr << "Including '" << graph.path(node)
                    << "' as it was not found in '" << ninjaLog << "'"
                    << std::endl;
        }
      } else if (hashMismatch[node]) {
        isAffected[node] = true;
        if (explain) {
          std::cerr << "Including '" << graph.path(node)
                    << "' as the build command hash differs in '" << ninjaLog
                    << "'" << std::endl;
        }
      }
    }
  }

  // If `node` has not been seen (using `seen`) then call
  // `markIfChildrenAffected` for all inputs to `node` and then set
  // `isAffected[node]` if any child is affected. Return whether this
  // NOLINTNEXTLINE(misc-no-recursion)
  void markIfChildrenAffected(Node node,
                              std::vector<bool>& seen,
                              bool explain) {
    if (seen[node]) {
      return;
    }
    seen[node] = true;

    // Always process all our children so that `isAffected` is updated for them
    const auto& inIndices = graph.in(node);
    for (const Graph::Node& in : inIndices) {
      markIfChildrenAffected(augment(in), seen, explain);
    }

    if (isAffected[node]) {
      return;
    }

    // Otherwise, find out if at least one of our children is affected and if
    // so, mark ourselves as affected
    const auto it =
        std::find_if(inIndices.begin(), inIndices.end(),
                     [&](const Graph::Node in) { return isAffected[in]; });
    if (it != inIndices.end()) {
      if (explain) {
        const std::optional<BuildCommandIndex>& commandIndex =
            nodeToCommand[node];
        // If we get to this place then we have children, therefore
        // we must have been built by a command
        assert(commandIndex.has_value());
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        const std::size_t idx = *commandIndex;

        // Only mention user-defined rules since built-in rules are always kept
        if (!isBuiltInRule(commands[idx].ruleIndex)) {
          std::cerr << "Including '" << graph.path(node)
                    << "' as it has the affected input '" << graph.path(*it)
                    << "'" << std::endl;
        }
      }
      isAffected[node] = true;
    }
  }

  // If `node` has not been seen (using `seen`) then call
  // `ifRequiredRequireAllChildren` for all outputs to `node` and then set
  // `isRequired[node]` if any child is required.
  // NOLINTNEXTLINE(misc-no-recursion)
  void ifRequiredRequireAllChildren(Node node,
                                    std::vector<bool>& seen,
                                    std::vector<bool>& needsAllInputs,
                                    bool explain) {
    if (seen[node]) {
      return;
    }
    seen[node] = true;

    for (const Graph::Node& out : graph.out(node)) {
      ifRequiredRequireAllChildren(augment(out), seen, needsAllInputs, explain);
    }

    // Nothing to do if we have no children
    if (graph.in(node).empty()) {
      return;
    }

    // If we get to this place then we have children, therefore
    // we must have been built by a command
    const std::optional<BuildCommandIndex>& commandIndex = nodeToCommand[node];
    assert(commandIndex.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const std::size_t idx = *commandIndex;

    if (!isBuiltInRule(commands[idx].ruleIndex)) {
      if (isRequired[node]) {
        needsAllInputs[node] = true;
        return;
      }
    }

    // If any build commands requiring us are marked as needing all inputs then
    // mark ourselves as affected and that we also need all our inputs.
    const auto& outIndices = graph.out(node);
    const auto it = std::find_if(
        outIndices.begin(), outIndices.end(),
        [&](const Graph::Node out) { return needsAllInputs[out]; });
    if (it != outIndices.end()) {
      if (!isRequired[node]) {
        if (explain) {
          std::cerr << "Including '" << graph.path(node)
                    << "' as it is a required input for the affected output '"
                    << graph.path(*it) << "'" << std::endl;
        }
        isRequired[node] = true;
      }
      needsAllInputs[node] = true;
    }
  }

  IndexIntoRange<Node> nodes() const {
    return IndexIntoRange<Node>{graph.nodes(), this};
  }
};

class Imp {
 public:
  ManagedResources buffer;
  BuildContext ctx;

  Imp() : buffer{}, ctx{buffer} {}
};

}  // namespace detail

TrimUtil::TrimUtil() : m_imp{nullptr} {}

TrimUtil::~TrimUtil() = default;

void TrimUtil::trim(std::ostream& output,
                    const std::filesystem::path& ninjaFile,
                    const std::string& ninjaFileContents,
                    std::istream& affected,
                    bool explain) {
  using namespace detail;

  // Keep our state inside `m_imp` so that we defer cleanup until the destructor
  // of `TrimUtil`. This allows the calling code to skip all destructors when
  // calling `std::_Exit`.
  m_imp = std::make_unique<Imp>();
  BuildContext& ctx = m_imp->ctx;

  // Parse the build file, this needs to be the first thing so we choose the
  // canonical paths in the same way that ninja does
  {
    const Timer t = CPUProfiler::start(".ninja parse");
    const std::string_view nullTerminated{ninjaFileContents.data(),
                                          ninjaFileContents.size() + 1};
    ctx.parse(ninjaFile, nullTerminated);
  }

  const std::filesystem::path ninjaFileDir =
      std::filesystem::path{ninjaFile}.remove_filename();

  const std::filesystem::path builddir = [&] {
    std::string path;
    ctx.fileScope.appendValue(path, "builddir");
    return ninjaFileDir / path;
  }();

  // Add all dynamic dependencies from `.ninja_deps` to the graph
  if (const std::filesystem::path ninjaDeps = builddir / ".ninja_deps";
      std::filesystem::exists(ninjaDeps)) {
    const Timer t = CPUProfiler::start(".ninja_deps parse");
    ctx.parseDepFile(ninjaDeps);
  }

  const Graph& graph = ctx.graph;

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
    ctx.isAffected.assign(ctx.isAffected.size(), true);
  } else {
    const Timer t = CPUProfiler::start(".ninja_log parse");
    ctx.parseLogFile(
        ninjaLog,
        [&](const BuildContext::Node& node) -> std::string_view {
          return ctx.commands[*ctx.nodeToCommand[node]].hashTarget;
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
      const std::optional<Graph::Node> node = graph.findPath(std::string{line});
      if (node.has_value()) {
        if (explain && !ctx.isAffected[*node]) {
          std::cerr << "Including '" << line
                    << "' as it was marked as affected by the user"
                    << std::endl;
        }
        ctx.isAffected[*node] = true;
        continue;
      }
    }

    // If that does not indicate a path, try the absolute path
    const std::filesystem::path p{line};
    if (!p.is_absolute()) {
      std::error_code error;
      const std::filesystem::path& absolute =
          attempted.emplace_back(std::filesystem::absolute(p, error));
      if (!error) {
        const std::optional<Graph::Node> node =
            graph.findPath(absolute.string());
        if (node.has_value()) {
          if (explain && !ctx.isAffected[*node]) {
            std::cerr << "Including '" << line
                      << "' as it was marked as affected by the user"
                      << std::endl;
          }
          ctx.isAffected[*node] = true;
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
        const std::optional<Graph::Node> node =
            graph.findPath(relative.string());
        if (node.has_value()) {
          if (explain && !ctx.isAffected[*node]) {
            std::cerr << "Including '" << line
                      << "' as it was marked as affected by the user"
                      << std::endl;
          }
          ctx.isAffected[*node] = true;
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
  for (const BuildContext::Node& node : ctx.nodes()) {
    ctx.markIfChildrenAffected(node, seen, explain);
  }

  // Keep the difference between build inputs that are affected (i.e. have
  // an input that is directly affected) or inputs that are required (i.e.
  // don't have an input that is directly affected but it itself is relied
  // on by an affected command)
  ctx.isRequired = ctx.isAffected;

  // Mark all inputs to affected outputs as required
  seen.assign(seen.size(), false);
  std::vector<bool> needsAllInputs(graph.size(), false);
  for (const BuildContext::Node& node : ctx.nodes()) {
    ctx.ifRequiredRequireAllChildren(node, seen, needsAllInputs, explain);
  }

  // Float all affected edges to the top so they are prioritized first
  // and Mark all required edges as needing to print them out
  for (const BuildContext::Node& node : ctx.nodes()) {
    if (ctx.isRequired[node]) {
      const std::optional commandIndex = ctx.nodeToCommand[node];
      if (commandIndex.has_value()) {
        BuildContext::BuildCommand& command = ctx.commands[*commandIndex];
        command.resolution = BuildContext::BuildCommand::Print;

        if (ctx.isAffected[node]) {
          for (const OutputText::iterator it : command.partIterators) {
            std::get<OutputText::FloatToTop>(*it) = OutputText::FloatToTop::Yes;
          }
        }
      }
    } else {
      // All affected indices are required
      assert(!ctx.isAffected[node]);
    }
  }

  // Go through all build commands, keep a note of rules that are needed and
  // `phony` out the build edges that weren't required.
  std::vector<bool> ruleReferenced(ctx.rules.size());
  for (BuildContext::BuildCommand& command : ctx.commands) {
    if (command.resolution == BuildContext::BuildCommand::Print) {
      ruleReferenced[command.ruleIndex] = true;
    } else {
      assert(command.resolution == BuildContext::BuildCommand::Phony);
      const std::initializer_list<std::string_view> parts = {
          command.outStr,
          command.validationStr.empty() ? ": phony" : ": phony ",
          command.validationStr,
          "\n",
      };
      std::pmr::string& phony = m_imp->buffer.createManagedString();
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
      assert(!command.partIterators.empty());

      std::get<std::string_view>(*command.partIterators.front()) = phony;
      std::for_each(
          std::next(command.partIterators.begin()), command.partIterators.end(),
          [&](const OutputText::iterator it) { ctx.output.parts.erase(it); });
      command.partIterators.clear();
    }
  }

  // Remove all rules that weren't referenced and float the remaining to
  // the top so they remain above any build commands we are rearranging
  // imminently
  for (BuildContext::RuleIndex ruleIndex{0, &ctx}; ruleIndex < ctx.rules.size();
       ++ruleIndex) {
    const Rule& rule = ctx.rules[ruleIndex];
    if (!ruleReferenced[ruleIndex]) {
      ctx.output.parts.erase(rule.begin(), rule.end());
    } else {
      std::for_each(rule.begin(), rule.end(), [&](auto& parts) {
        std::get<OutputText::FloatToTop>(parts) = OutputText::FloatToTop::Yes;
      });
    }
  }

  // Float marked parts to the top of the file, making sure not to cross over
  // any parts marked as immovable
  ctx.output.rearrange();
  trimTimer.stop();

  const Timer writeTimer = CPUProfiler::start("output time");
  std::transform(
      ctx.output.parts.begin(), ctx.output.parts.end(),
      std::ostream_iterator<std::string_view>{output},
      [](const auto& parts) { return std::get<std::string_view>(parts); });
}

// NOLINTEND(performance-avoid-endl)

}  // namespace trimja
