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

#ifndef TRIMJA_LOGREADER
#define TRIMJA_LOGREADER

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <iterator>
#include <string>

namespace trimja {

struct LogEntry {
  std::chrono::duration<std::int32_t, std::milli> startTime;
  std::chrono::duration<std::int32_t, std::milli> endTime;
  std::chrono::file_clock::time_point mtime;
  std::filesystem::path output;
  std::uint64_t hash;
};

class LogReader {
  std::istream* m_logs;
  std::string m_nextLine;

 public:
  struct sentinel {};

  class iterator {
    LogReader* m_reader;
    LogEntry m_entry;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = LogEntry;

    iterator(LogReader* reader);

    const LogEntry& operator*() const;

    iterator& operator++();
    void operator++(int);
    friend bool operator==(const iterator& iter, sentinel s);
    friend bool operator!=(const iterator& iter, sentinel s);
  };

 public:
  explicit LogReader(std::istream& logs);

  bool read(LogEntry* output);

  iterator begin();
  sentinel end();
};

}  // namespace trimja

#endif  // TRIMJA_LOGREADER
