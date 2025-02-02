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

#include "logreader.h"
#include "ninja_clock.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>

namespace trimja {
namespace {

template <std::size_t N>
std::array<std::string_view, N> splitOnTab(std::string_view in) {
  std::array<std::string_view, N> parts = {};
  auto it = in.begin();
  const auto end = in.end();
  for (std::string_view& part : parts) {
    const auto tab = std::find(it, end, '\t');
    part = std::string_view{&*it,
                            static_cast<std::string_view::size_type>(tab - it)};
    it = tab + std::min<std::size_t>(sizeof('\t'), end - tab);
  }
  return parts;
}

}  // namespace

static_assert(std::input_iterator<LogReader::iterator>);

LogReader::iterator::iterator(LogReader* reader) : m_reader(reader), m_entry() {
  ++(*this);
}

const LogEntry& LogReader::iterator::operator*() const {
  return m_entry;
}

LogReader::iterator& LogReader::iterator::operator++() {
  if (!m_reader->read(&m_entry)) {
    m_reader = nullptr;
  }
  return *this;
}

void LogReader::iterator::operator++(int) {
  ++*this;
}

bool operator==(const LogReader::iterator& iter, LogReader::sentinel) {
  return iter.m_reader == nullptr;
}

bool operator!=(const LogReader::iterator& iter, LogReader::sentinel) {
  return iter.m_reader != nullptr;
}

LogReader::LogReader(std::istream& logs, int fields)
    : m_logs{&logs},
      m_nextLine{},
      m_hashType{static_cast<HashType>(-1)},
      m_fields{fields} {
  std::getline(*m_logs, m_nextLine, '\n');
  const std::string_view prefix = "# ninja log v";
  if (!m_nextLine.starts_with(prefix)) {
    throw std::runtime_error{"Unable to find log file signature"};
  }

  const std::string_view versionStr =
      std::string_view{m_nextLine}.substr(prefix.size());
  // We support the following versions of the log file format:
  // * 5 has high-resolution timestamps
  // * 6 is identical https://github.com/ninja-build/ninja/pull/2240
  // * 7 only changes the hash function
  //   https://github.com/ninja-build/ninja/pull/2519
  if (versionStr != "5" && versionStr != "6" && versionStr != "7") {
    std::string msg;
    msg += "Unsupported log file version (";
    msg += versionStr;
    msg += ") found";
    throw std::runtime_error{msg};
  }

  assert(versionStr.size() == 1);
  m_hashType = versionStr[0] == '7' ? HashType::rapidhash : HashType::murmur;
}

bool LogReader::read(LogEntry* output) {
  std::getline(*m_logs, m_nextLine, '\n');
  if (m_nextLine.empty()) {
    return false;
  }

  std::array<std::string_view, 5> parts = splitOnTab<5>(m_nextLine);

  if (m_fields & LogEntry::Fields::startTime) {
    std::int32_t ticks;
    std::from_chars(parts[0].data(), parts[0].data() + parts[0].size(), ticks);
    output->startTime = std::chrono::duration<std::int32_t, std::milli>{ticks};
  }

  if (m_fields & LogEntry::Fields::endTime) {
    std::int32_t ticks;
    std::from_chars(parts[1].data(), parts[1].data() + parts[1].size(), ticks);
    output->endTime = std::chrono::duration<std::int32_t, std::milli>{ticks};
  }

  if (m_fields & LogEntry::Fields::mtime) {
    ninja_clock::rep ticks;
    std::from_chars(parts[2].data(), parts[2].data() + parts[2].size(), ticks);
    output->mtime = ninja_clock::to_file_clock(
        ninja_clock::time_point{ninja_clock::duration{ticks}});
  }

  if (m_fields & LogEntry::Fields::out) {
    output->out = parts[3];
  }

  if (m_fields & LogEntry::Fields::hash) {
    std::from_chars(parts[4].data(), parts[4].data() + parts[4].size(),
                    output->hash, 16);
    output->hashType = m_hashType;
  }
  return true;
}

LogReader::iterator LogReader::begin() {
  return iterator(this);
}

LogReader::sentinel LogReader::end() {
  return sentinel();
}

}  // namespace trimja
