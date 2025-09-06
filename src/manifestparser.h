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

#ifndef TRIMJA_PARSER
#define TRIMJA_PARSER

#include "evalstring.h"

#include <ninja/lexer.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace trimja {

namespace detail {

/**
 * @struct BaseReader
 * @brief Base class for reading tokens from a Ninja build file.
 */
struct BaseReader {
 protected:
  Lexer* m_lexer;
  EvalStringBuilder* m_storage;

 public:
  BaseReader(Lexer* lexer, EvalStringBuilder* storage);
  const char* position() const;
};

/**
 * @struct BaseReaderWithStart
 * @brief Base class for reading tokens with a start position.
 */
struct BaseReaderWithStart : public BaseReader {
 private:
  const char* m_start;

 public:
  BaseReaderWithStart(Lexer* lexer,
                      EvalStringBuilder* storage,
                      const char* start);
  const char* start() const;
  std::size_t bytesParsed() const;
};

}  // namespace detail

/**
 * @class VariableReader
 * @brief Class for reading variable definitions in a Ninja build file.
 */
class VariableReader : public detail::BaseReaderWithStart {
  std::string_view m_name;

 public:
  VariableReader(Lexer* lexer, EvalStringBuilder* storage, const char* start);
  std::string_view name() const;
  const EvalString& value() const;

  template <std::size_t I>
  std::tuple_element_t<I, VariableReader> get() const& {
    if constexpr (I == 0) {
      return m_name;
    }
    if constexpr (I == 1) {
      return m_storage->str();
    }
  }
};

/**
 * @class LetRangeReader
 * @brief Class for reading a range of variable definitions in a Ninja build
 * file.
 */
class LetRangeReader : public detail::BaseReaderWithStart {
 public:
  class sentinel {};
  class iterator : private detail::BaseReader {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = VariableReader;

    iterator(Lexer* lexer, EvalStringBuilder* storage);

    value_type operator*();
    iterator& operator++();
    void operator++(int);
    friend bool operator==(const iterator& iter, sentinel s);
    friend bool operator!=(const iterator& iter, sentinel s);
  };

 public:
  LetRangeReader(Lexer* lexer, EvalStringBuilder* storage);

  iterator begin();
  sentinel end() const;
};

/**
 * @class PathRangeReader
 * @brief Class for reading a range of paths in a Ninja build file.
 */
class PathRangeReader : public detail::BaseReader {
 public:
  class sentinel {};
  class iterator : private detail::BaseReader {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = EvalString;

   private:
    int m_expectedLastToken;

   public:
    iterator(Lexer* lexer, EvalStringBuilder* storage, int lastToken = -1);

    const value_type& operator*() const;

    iterator& operator++();
    void operator++(int);
    friend bool operator==(const iterator& iter, sentinel s);
    friend bool operator!=(const iterator& iter, sentinel s);
  };

 private:
  int m_expectedLastToken;

 public:
  PathRangeReader();
  PathRangeReader(Lexer* lexer, EvalStringBuilder* storage);
  PathRangeReader(Lexer* lexer,
                  EvalStringBuilder* storage,
                  Lexer::Token expectedLastToken);
  iterator begin();
  sentinel end() const;
};

/**
 * @class PoolReader
 * @brief Class for reading pool definitions in a Ninja build file.
 */
class PoolReader : public detail::BaseReaderWithStart {
  std::string_view m_name;

 public:
  PoolReader();  ///<  For private use
  PoolReader(Lexer* lexer, EvalStringBuilder* storage, const char* start);
  std::string_view name() const;
  LetRangeReader readVariables();
};

/**
 * @class BuildReader
 * @brief Class for reading build statements in a Ninja build file.
 */
class BuildReader : public detail::BaseReaderWithStart {
 public:
  using detail::BaseReaderWithStart::BaseReaderWithStart;
  PathRangeReader readOut();
  PathRangeReader readImplicitOut();
  std::string_view readName();
  PathRangeReader readIn();
  PathRangeReader readImplicitIn();
  PathRangeReader readOrderOnlyDeps();
  PathRangeReader readValidations();
  LetRangeReader readVariables();
};

/**
 * @class RuleReader
 * @brief Class for reading rule definitions in a Ninja build file.
 */
class RuleReader : public detail::BaseReaderWithStart {
  std::string_view m_name;

 public:
  RuleReader(Lexer* lexer, EvalStringBuilder* storage, const char* start);
  std::string_view name() const;
  LetRangeReader readVariables();
};

/**
 * @class DefaultReader
 * @brief Class for reading default statements in a Ninja build file.
 */
class DefaultReader : public detail::BaseReaderWithStart {
 public:
  using detail::BaseReaderWithStart::BaseReaderWithStart;
  PathRangeReader readPaths();
};

/**
 * @class IncludeReader
 * @brief Class for reading include statements in a Ninja build file.
 */
class IncludeReader : public detail::BaseReaderWithStart {
 public:
  IncludeReader(Lexer* lexer, EvalStringBuilder* storage, const char* start);
  const EvalString& path() const;

  // Return the path passed in to the `ManifestReader` constructor.
  const std::filesystem::path& parent() const;
};

/**
 * @class SubninjaReader
 * @brief Class for reading subninja statements in a Ninja build file.
 */
class SubninjaReader : public detail::BaseReaderWithStart {
 public:
  SubninjaReader(Lexer* lexer, EvalStringBuilder* storage, const char* start);
  const EvalString& path() const;

  // Return the path passed in to the `ManifestReader` constructor.
  const std::filesystem::path& parent() const;
};

/**
 * @class ManifestReader
 * @brief Class for parsing a Ninja build file.
 */
class ManifestReader {
  Lexer m_lexer;
  EvalStringBuilder m_storage;

 public:
  class sentinel {};
  class iterator : private detail::BaseReader {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::variant<PoolReader,
                                    BuildReader,
                                    RuleReader,
                                    DefaultReader,
                                    VariableReader,
                                    IncludeReader,
                                    SubninjaReader>;

   private:
    value_type m_value;

   public:
    iterator(Lexer* lexer, EvalStringBuilder* storage);

    value_type operator*() const;

    iterator& operator++();
    void operator++(int);
    friend bool operator==(const iterator& iter, sentinel s);
    friend bool operator!=(const iterator& iter, sentinel s);
  };

  ManifestReader(const std::filesystem::path& ninjaFile,
                 const std::string& ninjaFileContents);
  iterator begin();
  sentinel end();
};

}  // namespace trimja

namespace std {
template <>
struct tuple_size<trimja::VariableReader> {
  static constexpr size_t value = 2;
};

template <>
struct tuple_element<0, trimja::VariableReader> {
  using type = std::string_view;
};

template <>
struct tuple_element<1, trimja::VariableReader> {
  using type = const trimja::EvalString&;
};
}  // namespace std

#endif
