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
#include <iosfwd>
#include <iterator>
#include <string>
#include <string_view>

namespace trimja {

/**
 * @enum HashType
 * @brief Represents the type of hash function used to compute the build hash.
 */
enum class HashType {
  murmur,
  rapidhash,
};

/**
 * @struct LogEntry
 * @brief Represents a single log entry.
 *
 * This structure holds information about a single log entry, including
 * start and end times, modification time, output path, and the build hash
 * value.
 */
struct LogEntry {
  /**
   * @enum Fields
   * @brief Represents the fields that can be read from a log entry.
   */
  struct Fields {
    enum {
      startTime = 1 << 1,
      endTime = 1 << 2,
      mtime = 1 << 3,
      out = 1 << 4,
      hash = 1 << 5,
    };
  };

  std::chrono::duration<std::int32_t, std::milli> startTime;
  std::chrono::duration<std::int32_t, std::milli> endTime;
  std::chrono::file_clock::time_point mtime;
  std::string_view out;
  std::uint64_t hash;
  HashType hashType;
};

/**
 * @class LogReader
 * @brief Reads and parses log entries from an input stream.
 *
 * The LogReader class provides functionality to read and parse log entries
 * from a given input stream. It supports iteration over the log entries.
 */
class LogReader {
  std::istream* m_logs;
  std::string m_nextLine;
  HashType m_hashType;
  int m_fields;

 public:
  /**
   * @struct sentinel
   * @brief Sentinel type for the end of the iterator range.
   */
  struct sentinel {};

  /**
   * @class iterator
   * @brief Iterator for traversing log entries.
   *
   * The iterator class provides functionality to traverse log entries
   * read by the LogReader.
   */
  class iterator {
    LogReader* m_reader;
    LogEntry m_entry;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = LogEntry;

    /**
     * @brief Constructs an iterator for the given LogReader.
     * @param reader Pointer to the LogReader.
     */
    iterator(LogReader* reader);

    /**
     * @brief Dereferences the iterator to access the current log entry.
     * @return Reference to the current log entry.
     */
    const LogEntry& operator*() const;

    /**
     * @brief Advances the iterator to the next log entry.
     * @return Reference to the iterator.
     */
    iterator& operator++();

    /**
     * @brief Advances the iterator to the next log entry.
     */
    void operator++(int);

    /**
     * @brief Compares the iterator with a sentinel for equality.
     * @param iter The iterator to compare.
     * @param s The sentinel to compare.
     * @return Return whether the iterator has read past the end of the file.
     */
    friend bool operator==(const iterator& iter, sentinel s);

    /**
     * @brief Compares the iterator with a sentinel for inequality.
     * @param iter The iterator to compare.
     * @param s The sentinel to compare.
     * @return Return whether the iterator points to a valid entry, i.e. not
     * past the end of the file.
     */
    friend bool operator!=(const iterator& iter, sentinel s);
  };

 public:
  /**
   * @brief Constructs a LogReader with the given input stream.
   * @param logs The input stream to read log entries from.
   * @param fields The fields to read from the log entries.
   */
  explicit LogReader(std::istream& logs,
                     int fields = LogEntry::Fields::startTime |
                                  LogEntry::Fields::endTime |
                                  LogEntry::Fields::mtime |
                                  LogEntry::Fields::out |
                                  LogEntry::Fields::hash);

  /**
   * @brief Reads the next log entry from the input stream.
   * @param output Pointer to the LogEntry to store the read data.
   * @return Return true if an entry was successfully read and false if we are
   * at the end of the file.
   */
  bool read(LogEntry* output);

  /**
   * @brief Returns an iterator to the beginning of the log entries.
   * @return An iterator to the beginning of the log entries.
   */
  iterator begin();

  /**
   * @brief Returns a sentinel representing the end of the log entries.
   * @return A sentinel representing the end of the log entries.
   */
  sentinel end();
};

}  // namespace trimja

#endif  // TRIMJA_LOGREADER
