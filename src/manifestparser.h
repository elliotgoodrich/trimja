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

#include <ninja/eval_env.h>
#include <ninja/lexer.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>

namespace trimja {

namespace detail {

struct BaseReader {
 protected:
  Lexer* m_lexer;
  EvalString* m_storage;

 public:
  BaseReader(Lexer* lexer, EvalString* storage);
  const char* position() const;
};

struct BaseReaderWithStart : public BaseReader {
 private:
  const char* m_start;

 public:
  BaseReaderWithStart(Lexer* lexer, EvalString* storage, const char* start);
  const char* start() const;
  std::size_t bytesParsed() const;
};

}  // namespace detail

class VariableReader : public detail::BaseReaderWithStart {
 public:
  using detail::BaseReaderWithStart::BaseReaderWithStart;
  std::string_view name();
  const EvalString& value();
};

class LetRangeReader : public detail::BaseReaderWithStart {
 public:
  class sentinel {};
  class iterator : private detail::BaseReader {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = VariableReader;

    iterator(Lexer* lexer, EvalString* storage);

    value_type operator*();
    iterator& operator++();
    void operator++(int);
    friend bool operator==(const iterator& iter, sentinel s);
    friend bool operator!=(const iterator& iter, sentinel s);
  };

 public:
  LetRangeReader(Lexer* lexer, EvalString* storage);

  iterator begin();
  sentinel end() const;
};

class PathRangeReader : public detail::BaseReader {
 public:
  class sentinel {};
  class iterator : private detail::BaseReader {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = EvalString;

   private:
    Lexer::Token m_expectedLastToken;

   public:
    iterator(Lexer* lexer, value_type* storage);
    iterator(Lexer* lexer, value_type* storage, Lexer::Token lastToken);

    const value_type& operator*() const;

    iterator& operator++();
    void operator++(int);
    friend bool operator==(const iterator& iter, sentinel s);
    friend bool operator!=(const iterator& iter, sentinel s);
  };

 private:
  Lexer::Token m_expectedLastToken;

 public:
  PathRangeReader();
  PathRangeReader(Lexer* lexer, EvalString* storage);
  PathRangeReader(Lexer* lexer,
                  EvalString* storage,
                  Lexer::Token expectedLastToken);
  iterator begin();
  sentinel end() const;
};

class PoolReader : public detail::BaseReaderWithStart {
 public:
  using detail::BaseReaderWithStart::BaseReaderWithStart;
  std::string_view name();
  LetRangeReader variables();
};

class BuildReader : public detail::BaseReaderWithStart {
 public:
  using detail::BaseReaderWithStart::BaseReaderWithStart;
  PathRangeReader out();
  PathRangeReader implicitOut();
  std::string_view name();
  PathRangeReader in();
  PathRangeReader implicitIn();
  PathRangeReader orderOnlyDeps();
  PathRangeReader validations();
  LetRangeReader variables();
};

class RuleReader : public detail::BaseReaderWithStart {
 public:
  using detail::BaseReaderWithStart::BaseReaderWithStart;
  std::string_view name();
  LetRangeReader variables();
};

class DefaultReader : public detail::BaseReaderWithStart {
 public:
  using detail::BaseReaderWithStart::BaseReaderWithStart;
  PathRangeReader paths();
};

class IncludeReader : public detail::BaseReaderWithStart {
 public:
  using detail::BaseReaderWithStart::BaseReaderWithStart;
  const EvalString& path();

  // Return the path passed in to the `ManifestReader` constructor. Note that
  // this method can be called before or after `path`.
  const std::filesystem::path& parent() const;
};

class SubninjaReader : public detail::BaseReaderWithStart {
 public:
  using detail::BaseReaderWithStart::BaseReaderWithStart;

  // Return the path passed in to the `ManifestReader` constructor. Note that
  // this method can be called before or after `path`.
  const std::filesystem::path& parent() const;
};

class ManifestReader {
  Lexer m_lexer;
  EvalString m_storage;

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
    iterator(Lexer* lexer, EvalString* storage);

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

#endif
