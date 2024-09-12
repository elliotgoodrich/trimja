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

struct PathRecordView {
  std::int32_t index;
  std::string_view path;
};

struct DepsRecordView {
  std::int32_t outIndex;
  std::chrono::file_clock::time_point mtime;
  std::span<const std::int32_t> deps;
};

class DepsReader {
  std::ifstream m_deps;
  std::string m_storage;
  std::vector<std::int32_t> m_depsStorage;
  std::filesystem::path m_filePath;

 public:
  explicit DepsReader(const std::filesystem::path& ninja_deps);

  std::variant<PathRecordView, DepsRecordView, std::monostate> read();
};

}  // namespace trimja

#endif  // TRIMJA_DEPSREADER
