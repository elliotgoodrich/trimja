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

#include "depsreader.h"
#include "graph.h"

#include <algorithm>
#include <ios>
#include <istream>
#include <span>
#include <vector>

namespace trimja {
namespace {

// Ninja's maximum record size, we will try and respect it
const std::size_t NINJA_MAX_RECORD_SIZE = 0b11'1111'1111'1111'1111;

template <typename TYPE>
TYPE readFromStream(std::istream& in) {
  TYPE value;
  in.read(reinterpret_cast<char*>(&value), sizeof(value));
  return value;
}

}  // namespace

static_assert(std::input_iterator<DepsReader::iterator>);

DepsReader::iterator::iterator(DepsReader* reader)
    : m_reader(reader), m_entry() {
  ++*this;
}

const DepsReader::iterator::value_type& DepsReader::iterator::operator*()
    const {
  return m_entry;
}

DepsReader::iterator& DepsReader::iterator::operator++() {
  if (!m_reader->read(&m_entry)) {
    m_reader = nullptr;
  }
  return *this;
}

void DepsReader::iterator::operator++(int) {
  ++*this;
}

bool operator==(const DepsReader::iterator& iter, DepsReader::sentinel) {
  return iter.m_reader == nullptr;
}

bool operator!=(const DepsReader::iterator& iter, DepsReader::sentinel) {
  return iter.m_reader != nullptr;
}

DepsReader::DepsReader(const std::filesystem::path& ninja_deps)
    : m_deps(ninja_deps, std::ios_base::binary),
      m_storage(),
      m_depsStorage(),
      m_filePath(ninja_deps) {
  m_deps.exceptions(std::ios_base::failbit | std::ios_base::badbit);
  std::getline(m_deps, m_storage);
  if (m_storage != "# ninjadeps") {
    throw std::runtime_error("Unable to find ninjadeps signature");
  }

  const auto header = readFromStream<std::int32_t>(m_deps);
  if (header != 4) {
    throw std::runtime_error("Only version 4 supported of ninjadeps");
  }
}

bool DepsReader::read(std::variant<PathRecordView, DepsRecordView>* output) {
  try {
    if (m_deps.peek() == EOF) {
      return false;
    }
    const auto rawRecordSize = readFromStream<std::uint32_t>(m_deps);
    const std::uint32_t recordSize = rawRecordSize & 0x7FFFFFFF;
    if (recordSize > NINJA_MAX_RECORD_SIZE) {
      throw std::runtime_error("Record exceeding the maximum size found");
    }
    if (const bool isPathRecord = (rawRecordSize >> 31) == 0) {
      const std::uint32_t pathSize = recordSize - sizeof(std::uint32_t);
      m_storage.resize(pathSize);
      m_deps.read(m_storage.data(), pathSize);
      const auto end = m_storage.end();
      const std::size_t padding =
          (end[-1] == '\0') + (end[-2] == '\0') + (end[-3] == '\0');
      m_storage.erase(m_storage.size() - padding);
      const auto checksum = readFromStream<std::uint32_t>(m_deps);
      const std::int32_t id = ~checksum;
      *output = PathRecordView{id, m_storage};
    } else {
      const auto outIndex = readFromStream<std::int32_t>(m_deps);
      const auto mtime = readFromStream<ninja_clock::time_point>(m_deps);
      const std::uint32_t numDependencies =
          (recordSize - sizeof(std::int32_t) - 2 * sizeof(std::uint32_t)) /
          sizeof(std::int32_t);
      m_depsStorage.resize(numDependencies);
      std::generate_n(m_depsStorage.begin(), numDependencies,
                      [&] { return readFromStream<std::int32_t>(m_deps); });
      *output = DepsRecordView{outIndex, mtime, m_depsStorage};
    }
  } catch (const std::ios_base::failure& e) {
    std::string msg;
    msg += "Error reading ";
    msg += m_filePath.string();
    msg += ": ";
    msg += e.what();
    throw std::runtime_error(msg);
  }

  return true;
}

DepsReader::iterator DepsReader::begin() {
  return iterator(this);
}

DepsReader::sentinel DepsReader::end() {
  return sentinel();
}

}  // namespace trimja
