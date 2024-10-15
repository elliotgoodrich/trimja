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

BaseReader::BaseReader(Lexer* lexer, EvalString* storage)
    : m_lexer(lexer), m_storage(storage) {}

const char* BaseReader::position() const {
  return m_lexer->position();
}

BaseReaderWithStart::BaseReaderWithStart(Lexer* lexer,
                                         EvalString* storage,
                                         const char* start)
    : BaseReader{lexer, storage}, m_start{start} {}

const char* BaseReaderWithStart::start() const {
  return m_start;
}

std::size_t BaseReaderWithStart::bytesParsed() const {
  return position() - m_start;
}

}  // namespace detail

std::string_view VariableReader::name() {
  std::string_view name;
  if (!m_lexer->ReadIdent(&name)) {
    throw std::runtime_error("Missing variable name");
  }
  return name;
}

const EvalString& VariableReader::value() {
  expectToken(m_lexer, Lexer::EQUALS);

  std::string err;
  m_storage->clear();
  if (!m_lexer->ReadVarValue(m_storage, &err)) {
    throw std::runtime_error(err);
  }
  return *m_storage;
}

LetRangeReader::iterator::iterator(Lexer* lexer, EvalString* storage)
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

LetRangeReader::LetRangeReader(Lexer* lexer, EvalString* storage)
    : detail::BaseReaderWithStart{lexer, storage, lexer->position()} {}

LetRangeReader::iterator LetRangeReader::begin() {
  return iterator{m_lexer, m_storage};
}

LetRangeReader::sentinel LetRangeReader::end() const {
  return sentinel{};
}

PathRangeReader::iterator::iterator(Lexer* lexer, EvalString* storage)
    : iterator(lexer, storage, static_cast<Lexer::Token>(-1)) {}

PathRangeReader::iterator::iterator(Lexer* lexer,
                                    EvalString* storage,
                                    Lexer::Token lastToken)
    : detail::BaseReader(lexer, storage), m_expectedLastToken(lastToken) {
  if (m_lexer) {
    ++*this;
  }
}

const PathRangeReader::iterator::value_type&
PathRangeReader::iterator::operator*() const {
  return *m_storage;
}

PathRangeReader::iterator& PathRangeReader::iterator::operator++() {
  std::string err;
  m_storage->clear();
  if (!m_lexer->ReadPath(m_storage, &err)) {
    throw std::runtime_error(err);
  }
  if (m_storage->empty()) {
    if (static_cast<int>(m_expectedLastToken) != -1) {
      expectToken(m_lexer, m_expectedLastToken);
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

PathRangeReader::PathRangeReader(Lexer* lexer, EvalString* storage)
    : PathRangeReader(lexer, storage, static_cast<Lexer::Token>(-1)) {}

PathRangeReader::PathRangeReader(Lexer* lexer,
                                 EvalString* storage,
                                 Lexer::Token expectedLastToken)
    : detail::BaseReader(lexer, storage),
      m_expectedLastToken(expectedLastToken) {}

PathRangeReader::iterator PathRangeReader::begin() {
  return iterator{m_lexer, m_storage, m_expectedLastToken};
}

PathRangeReader::sentinel PathRangeReader::end() const {
  return sentinel{};
}

std::string_view PoolReader::name() {
  std::string_view name;
  if (!m_lexer->ReadIdent(&name)) {
    throw std::runtime_error("Missing name for pool");
  }

  expectToken(m_lexer, Lexer::NEWLINE);
  return name;
}

LetRangeReader PoolReader::variables() {
  return LetRangeReader{m_lexer, m_storage};
};

PathRangeReader BuildReader::out() {
  return PathRangeReader{m_lexer, m_storage};
}

PathRangeReader BuildReader::implicitOut() {
  return m_lexer->PeekToken(Lexer::PIPE) ? PathRangeReader{m_lexer, m_storage}
                                         : PathRangeReader{};
}

std::string_view BuildReader::name() {
  expectToken(m_lexer, Lexer::COLON);
  std::string_view name;
  if (!m_lexer->ReadIdent(&name)) {
    throw std::runtime_error("Missing rule name for build command");
  }
  return name;
}

PathRangeReader BuildReader::in() {
  return PathRangeReader{m_lexer, m_storage};
}

PathRangeReader BuildReader::implicitIn() {
  return m_lexer->PeekToken(Lexer::PIPE) ? PathRangeReader{m_lexer, m_storage}
                                         : PathRangeReader{};
}

PathRangeReader BuildReader::orderOnlyDeps() {
  return m_lexer->PeekToken(Lexer::PIPE2) ? PathRangeReader{m_lexer, m_storage}
                                          : PathRangeReader{};
}

PathRangeReader BuildReader::validations() {
  return m_lexer->PeekToken(Lexer::PIPEAT) ? PathRangeReader{m_lexer, m_storage}
                                           : PathRangeReader{};
}

LetRangeReader BuildReader::variables() {
  expectToken(m_lexer, Lexer::NEWLINE);
  return LetRangeReader{m_lexer, m_storage};
}

std::string_view RuleReader::name() {
  std::string_view name;
  if (!m_lexer->ReadIdent(&name)) {
    throw std::runtime_error("Missing name for rule");
  }

  expectToken(m_lexer, Lexer::NEWLINE);
  return name;
}

LetRangeReader RuleReader::variables() {
  return LetRangeReader{m_lexer, m_storage};
};

PathRangeReader DefaultReader::paths() {
  return PathRangeReader{m_lexer, m_storage, Lexer::NEWLINE};
}

const EvalString& IncludeReader::path() {
  std::string err;
  m_storage->clear();
  if (!m_lexer->ReadPath(m_storage, &err)) {
    throw std::runtime_error(err);
  }
  expectToken(m_lexer, Lexer::NEWLINE);
  return *m_storage;
}

const std::filesystem::path& IncludeReader::parent() const {
  return m_lexer->getFilename();
}

const std::filesystem::path& SubninjaReader::parent() const {
  return m_lexer->getFilename();
}

ManifestReader::iterator::iterator(Lexer* lexer, EvalString* storage)
    : detail::BaseReader{lexer, storage},
      m_value{PoolReader{nullptr, nullptr, nullptr}} {
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
                               const std::string& ninjaFileContents)
    : m_lexer(), m_storage() {
  m_lexer.Start(ninjaFile, ninjaFileContents);
}

ManifestReader::iterator ManifestReader::begin() {
  return iterator{&m_lexer, &m_storage};
}

ManifestReader::sentinel ManifestReader::end() {
  return sentinel{};
}

}  // namespace trimja
