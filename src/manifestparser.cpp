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

#include "manifestparser.h"

#include <ninja/lexer.h>

#include <stdexcept>

namespace trimja {

namespace {

void consume(auto&& range) {
  for ([[maybe_unused]] auto&& _ : range) {
  }
}

void expectToken(Lexer* lexer, Lexer::Token expected) {
  const Lexer::Token token = lexer->ReadToken();
  if (token != expected) {
    std::string msg;
    msg += "Expected ";
    msg += Lexer::TokenName(expected);
    msg += " but got ";
    msg += Lexer::TokenName(token);
    throw std::runtime_error(msg);
  }
}

}  // namespace

namespace detail {

BaseReader::BaseReader(Lexer* lexer, EvalStringBuilder* storage)
    : m_lexer(lexer), m_storage(storage) {}

const char* BaseReader::position() const {
  return m_lexer->position();
}

BaseReaderWithStart::BaseReaderWithStart(Lexer* lexer,
                                         EvalStringBuilder* storage,
                                         const char* start)
    : BaseReader{lexer, storage}, m_start{start} {}

const char* BaseReaderWithStart::start() const {
  return m_start;
}

std::size_t BaseReaderWithStart::bytesParsed() const {
  return position() - m_start;
}

}  // namespace detail

VariableReader::VariableReader(Lexer* lexer,
                               EvalStringBuilder* storage,
                               const char* start)
    : detail::BaseReaderWithStart{lexer, storage, start}, m_name{} {
  if (!m_lexer->ReadIdent(&m_name)) {
    throw std::runtime_error("Missing variable name");
  }

  expectToken(m_lexer, Lexer::EQUALS);
  std::string err;
  m_storage->clear();
  if (!m_lexer->ReadVarValue(m_storage, &err)) {
    throw std::runtime_error(err);
  }
}

std::string_view VariableReader::name() const {
  return m_name;
}

const EvalString& VariableReader::value() const {
  return m_storage->str();
}

void VariableReader::skip() {
  // Does nothing since we read during the constructor
}

LetRangeReader::iterator::iterator(Lexer* lexer, EvalStringBuilder* storage)
    : detail::BaseReader{lexer, storage} {
  ++*this;
}

VariableReader LetRangeReader::iterator::operator*() {
  return VariableReader{m_lexer, m_storage, m_lexer->position()};
}

LetRangeReader::iterator& LetRangeReader::iterator::operator++() {
  if (!m_lexer->PeekToken(Lexer::INDENT)) {
    m_lexer = nullptr;
  }

  return *this;
}

void LetRangeReader::iterator::operator++(int) {
  ++*this;
}

bool operator==(const LetRangeReader::iterator& iter,
                LetRangeReader::sentinel) {
  return iter.m_lexer == nullptr;
}

bool operator!=(const LetRangeReader::iterator& iter,
                LetRangeReader::sentinel s) {
  return !(iter == s);
}

LetRangeReader::LetRangeReader(Lexer* lexer, EvalStringBuilder* storage)
    : detail::BaseReaderWithStart{lexer, storage, lexer->position()} {}

LetRangeReader::iterator LetRangeReader::begin() {
  return iterator{m_lexer, m_storage};
}

LetRangeReader::sentinel LetRangeReader::end() const {
  return sentinel{};
}

void LetRangeReader::skip() {
  consume(*this);
}

PathRangeReader::iterator::iterator(Lexer* lexer,
                                    EvalStringBuilder* storage,
                                    const std::optional<int>& lastToken)
    : detail::BaseReader{lexer, storage}, m_expectedLastToken{lastToken} {
  if (m_lexer) {
    ++*this;
  }
}

const PathRangeReader::iterator::value_type&
PathRangeReader::iterator::operator*() const {
  return m_storage->str();
}

PathRangeReader::iterator& PathRangeReader::iterator::operator++() {
  std::string err;
  m_storage->clear();
  if (!m_lexer->ReadPath(m_storage, &err)) {
    throw std::runtime_error(err);
  }
  if (m_storage->str().empty()) {
    if (m_expectedLastToken.has_value()) {
      expectToken(m_lexer, static_cast<Lexer::Token>(*m_expectedLastToken));
    }
    m_lexer = nullptr;
  }
  return *this;
}

void PathRangeReader::iterator::operator++(int) {
  ++*this;
}

bool operator==(const PathRangeReader::iterator& iter,
                PathRangeReader::sentinel) {
  return iter.m_lexer == nullptr;
}

bool operator!=(const PathRangeReader::iterator& iter,
                PathRangeReader::sentinel s) {
  return !(iter == s);
}

PathRangeReader::PathRangeReader() : PathRangeReader{nullptr, nullptr} {}

PathRangeReader::PathRangeReader(Lexer* lexer, EvalStringBuilder* storage)
    : detail::BaseReader{lexer, storage}, m_expectedLastToken{} {}

PathRangeReader::PathRangeReader(Lexer* lexer,
                                 EvalStringBuilder* storage,
                                 Lexer::Token expectedLastToken)
    : detail::BaseReader{lexer, storage},
      m_expectedLastToken{expectedLastToken} {}

PathRangeReader::iterator PathRangeReader::begin() {
  return iterator{m_lexer, m_storage, m_expectedLastToken};
}

PathRangeReader::sentinel PathRangeReader::end() const {
  return sentinel{};
}

void PathRangeReader::skip() {
  consume(*this);
}

PoolReader::PoolReader()
    : detail::BaseReaderWithStart{nullptr, nullptr, nullptr}, m_name{} {}

PoolReader::PoolReader(Lexer* lexer,
                       EvalStringBuilder* storage,
                       const char* start)
    : detail::BaseReaderWithStart{lexer, storage, start}, m_name{} {
  if (!m_lexer->ReadIdent(&m_name)) {
    throw std::runtime_error("Missing name for pool");
  }

  expectToken(m_lexer, Lexer::NEWLINE);
}

std::string_view PoolReader::name() const {
  return m_name;
}

LetRangeReader PoolReader::readVariables() {
  return LetRangeReader{m_lexer, m_storage};
};

void PoolReader::skip() {
  consume(readVariables());
}

PathRangeReader BuildReader::readOut() {
  return PathRangeReader{m_lexer, m_storage};
}

PathRangeReader BuildReader::readImplicitOut() {
  return m_lexer->PeekToken(Lexer::PIPE) ? PathRangeReader{m_lexer, m_storage}
                                         : PathRangeReader{};
}

std::string_view BuildReader::readName() {
  expectToken(m_lexer, Lexer::COLON);
  std::string_view name;
  if (!m_lexer->ReadIdent(&name)) {
    throw std::runtime_error("Missing rule name for build command");
  }
  return name;
}

PathRangeReader BuildReader::readIn() {
  return PathRangeReader{m_lexer, m_storage};
}

PathRangeReader BuildReader::readImplicitIn() {
  return m_lexer->PeekToken(Lexer::PIPE) ? PathRangeReader{m_lexer, m_storage}
                                         : PathRangeReader{};
}

PathRangeReader BuildReader::readOrderOnlyDeps() {
  return m_lexer->PeekToken(Lexer::PIPE2) ? PathRangeReader{m_lexer, m_storage}
                                          : PathRangeReader{};
}

PathRangeReader BuildReader::readValidations() {
  return m_lexer->PeekToken(Lexer::PIPEAT) ? PathRangeReader{m_lexer, m_storage}
                                           : PathRangeReader{};
}

LetRangeReader BuildReader::readVariables() {
  expectToken(m_lexer, Lexer::NEWLINE);
  return LetRangeReader{m_lexer, m_storage};
}

void BuildReader::skip() {
  consume(readOut());
  consume(readImplicitOut());
  [[maybe_unused]] const std::string_view ruleName = readName();
  consume(readIn());
  consume(readImplicitIn());
  consume(readOrderOnlyDeps());
  consume(readValidations());
  consume(readVariables());
}

RuleReader::RuleReader(Lexer* lexer,
                       EvalStringBuilder* storage,
                       const char* start)
    : detail::BaseReaderWithStart{lexer, storage, start}, m_name{} {
  if (!m_lexer->ReadIdent(&m_name)) {
    throw std::runtime_error("Missing name for rule");
  }

  expectToken(m_lexer, Lexer::NEWLINE);
}

std::string_view RuleReader::name() const {
  return m_name;
}

LetRangeReader RuleReader::readVariables() {
  return LetRangeReader{m_lexer, m_storage};
};

void RuleReader::skip() {
  consume(readVariables());
}

PathRangeReader DefaultReader::readPaths() {
  return PathRangeReader{m_lexer, m_storage, Lexer::NEWLINE};
}

void DefaultReader::skip() {
  consume(readPaths());
}

IncludeReader::IncludeReader(Lexer* lexer,
                             EvalStringBuilder* storage,
                             const char* start)
    : detail::BaseReaderWithStart{lexer, storage, start} {
  std::string err;
  m_storage->clear();
  if (!m_lexer->ReadPath(m_storage, &err)) {
    throw std::runtime_error(err);
  }
  expectToken(m_lexer, Lexer::NEWLINE);
}

const EvalString& IncludeReader::path() const {
  return m_storage->str();
}

const std::filesystem::path& IncludeReader::parent() const {
  return m_lexer->getFilename();
}

void IncludeReader::skip() {
  // Does nothing since we read during the constructor
}

SubninjaReader::SubninjaReader(Lexer* lexer,
                               EvalStringBuilder* storage,
                               const char* start)
    : detail::BaseReaderWithStart{lexer, storage, start} {
  std::string err;
  m_storage->clear();
  if (!m_lexer->ReadPath(m_storage, &err)) {
    throw std::runtime_error(err);
  }
  expectToken(m_lexer, Lexer::NEWLINE);
}

const EvalString& SubninjaReader::path() const {
  return m_storage->str();
}

const std::filesystem::path& SubninjaReader::parent() const {
  return m_lexer->getFilename();
}

void SubninjaReader::skip() {
  // Does nothing since we read during the constructor
}

ManifestReader::iterator::iterator(Lexer* lexer, EvalStringBuilder* storage)
    : detail::BaseReader{lexer, storage}, m_value{PoolReader{}} {
  // We need to set `m_value` to anything so just choose `PoolReader`
  ++*this;
}

ManifestReader::iterator::value_type ManifestReader::iterator::operator*()
    const {
  return m_value;
}

ManifestReader::iterator& ManifestReader::iterator::operator++() {
  Lexer::Token token;
  const char* start;
  do {
    start = m_lexer->position();
  } while ((token = m_lexer->ReadToken()) == Lexer::NEWLINE);

  switch (token) {
    case Lexer::POOL:
      m_value.emplace<PoolReader>(m_lexer, m_storage, start);
      break;
    case Lexer::BUILD:
      m_value.emplace<BuildReader>(m_lexer, m_storage, start);
      break;
    case Lexer::RULE:
      m_value.emplace<RuleReader>(m_lexer, m_storage, start);
      break;
    case Lexer::DEFAULT:
      m_value.emplace<DefaultReader>(m_lexer, m_storage, start);
      break;
    case Lexer::IDENT:
      m_lexer->UnreadToken();
      m_value.emplace<VariableReader>(m_lexer, m_storage, start);
      break;
    case Lexer::INCLUDE:
      m_value.emplace<IncludeReader>(m_lexer, m_storage, start);
      break;
    case Lexer::SUBNINJA:
      m_value.emplace<SubninjaReader>(m_lexer, m_storage, start);
      break;
    case Lexer::ERROR:
      throw std::runtime_error("Parsing error");
    case Lexer::TEOF:
      m_lexer = nullptr;
      break;
    default: {
      std::string msg;
      msg += "Unexpected token ";
      msg += Lexer::TokenName(token);
      throw std::runtime_error(msg);
    }
  }

  return *this;
}

void ManifestReader::iterator::operator++(int) {
  ++*this;
}

bool operator==(const ManifestReader::iterator& iter,
                ManifestReader::sentinel) {
  return iter.m_lexer == nullptr;
}

bool operator!=(const ManifestReader::iterator& iter,
                ManifestReader::sentinel s) {
  return !(iter == s);
}

ManifestReader::ManifestReader(const std::filesystem::path& ninjaFile,
                               std::string_view ninjaFileContents)
    : m_lexer(), m_storage() {
  assert(ninjaFileContents.back() == '\0');
  m_lexer.Start(ninjaFile, ninjaFileContents);
}

ManifestReader::ManifestReader(const std::filesystem::path& ninjaFile,
                               const std::string& ninjaFileContents)
    : ManifestReader{ninjaFile,
                     std::string_view{ninjaFileContents.data(),
                                      ninjaFileContents.size() + 1}} {}

ManifestReader::iterator ManifestReader::begin() {
  return iterator{&m_lexer, &m_storage};
}

ManifestReader::sentinel ManifestReader::end() {
  return sentinel{};
}

}  // namespace trimja
