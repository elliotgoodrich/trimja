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

#ifndef TRIMJA_DEPSREADER
#define TRIMJA_DEPSREADER

#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
#include <variant>
#include <vector>

namespace trimja {

class Graph;

/**
 * @struct PathRecordView
 * @brief Represents a view of a path record inside .ninja_deps
 */
struct PathRecordView {
  std::int32_t index;
  std::string_view path;
};

/**
 * @struct DepsRecordView
 * @brief Represents a view of a dependency record inside .ninja_deps
 */
struct DepsRecordView {
  std::int32_t outIndex;
  std::chrono::file_clock::time_point mtime;
  std::span<const std::int32_t> deps;
};

/**
 * @class DepsReader
 * @brief Reads dependency records from a .ninja_deps file
 */
class DepsReader {
  std::ifstream m_deps;
  std::string m_storage;
  std::vector<std::int32_t> m_depsStorage;
  std::filesystem::path m_filePath;

 public:
  /**
   * @struct sentinel
   * @brief Sentinel type for the end of the iterator range.
   */
  struct sentinel {};

  /**
   * @class iterator
   * @brief Iterator for traversing dependency records in the deps file.
   */
  class iterator {
    DepsReader* m_reader;
    std::variant<PathRecordView, DepsRecordView> m_entry;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::variant<PathRecordView, DepsRecordView>;

    /**
     * @brief Constructs an iterator for the given DepsReader.
     * @param reader Pointer to the DepsReader.
     */
    iterator(DepsReader* reader);

    /**
     * @brief Dereferences the iterator to get the current entry.
     * @return The current entry.
     */
    const value_type& operator*() const;

    /**
     * @brief Advances the iterator to the next entry in the file.
     * @return Reference to the iterator.
     */
    iterator& operator++();

    /**
     * @brief Advances the iterator to the next entry.
     */
    void operator++(int);

    /**
     * @brief Compares the iterator with a sentinel for equality.
     * @param iter The iterator to compare.
     * @param s The sentinel to compare.
     * @return Whether the iterator has read past the end of the file.
     */
    friend bool operator==(const iterator& iter, sentinel s);

    /**
     * @brief Compares the iterator with a sentinel for inequality.
     * @param iter The iterator to compare.
     * @param s The sentinel to compare.
     * @return Whether the iterator points to a valid entry, i.e. not past the
     * end of the file.
     */
    friend bool operator!=(const iterator& iter, sentinel s);
  };

 public:
  /**
   * @brief Constructs a DepsReader for the given Ninja deps file.
   * @param ninja_deps The path to the Ninja .ninja_dep file.
   */
  explicit DepsReader(const std::filesystem::path& ninja_deps);

  /**
   * @brief Reads the next dependency record from the deps file.
   * @param output Pointer to the variant to store the read record.
   * @return Whether a record was successfully read.
   */
  bool read(std::variant<PathRecordView, DepsRecordView>* output);

  /**
   * @brief Gets an iterator to the beginning of the dependency records.
   * @return An iterator to the beginning.
   */
  iterator begin();

  /**
   * @brief Gets a sentinel representing the end of the dependency records.
   * @return A sentinel representing the end.
   */
  sentinel end();
};

}  // namespace trimja

#endif  // TRIMJA_DEPSREADER
