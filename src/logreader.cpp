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

template <typename T>
void parse(std::string_view name,
           T& out,
           const char*& begin,
           const char* end,
           int base = 10) {
  const auto [ptr, ec] = std::from_chars(begin, end, out, base);
  if (ec != std::errc{}) {
    std::string msg = "Failed to parse ";
    msg += name;
    throw std::runtime_error{msg};
  }
  if (ptr == end || *ptr != '\t') {
    std::string msg = "Expected ";
    msg += name;
    msg += " to be followed by a tab";
    throw std::runtime_error{msg};
  }
  begin = ptr + 1;
}

void skipAhead(std::string_view name, const char*& begin, const char* end) {
  begin = std::find(begin, end, '\t');
  if (begin == end) {
    std::string msg = "Unable to find ";
    msg += name;
    msg += " delimeter";
    throw std::runtime_error{msg};
  }
  ++begin;
}

}  // namespace

static_assert(std::input_iterator<LogReader::iterator>);

LogReader::iterator::iterator(LogReader* reader) : m_reader{reader}, m_entry{} {
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
      m_fields{fields},
      m_version{-1},
      m_lineNumber{1} {
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
    std::string msg = "Unsupported log file version (";
    msg += versionStr;
    msg += ") found";
    throw std::runtime_error{msg};
  }

  assert(versionStr.size() == 1);
  m_version = versionStr[0] - '0';
  m_hashType = m_version == 7 ? HashType::rapidhash : HashType::murmur;
}

int LogReader::version() const {
  return m_version;
}

bool LogReader::read(LogEntry* output) {
  m_nextLine = "";
  std::getline(*m_logs, m_nextLine, '\n');
  ++m_lineNumber;
  if (m_nextLine.empty()) {
    return false;
  }

  // Append a tab to the end of the line so that each entry is tab delimited
  // and this simplifies parsing.
  m_nextLine += '\t';

  const char* begin = m_nextLine.data();
  const char* end = begin + m_nextLine.size();

  if (m_fields & LogEntry::Fields::startTime) {
    std::int32_t ticks;
    parse("start time", ticks, begin, end);
    output->startTime = std::chrono::duration<std::int32_t, std::milli>{ticks};
  } else {
    skipAhead("start time", begin, end);
  }

  if (m_fields & LogEntry::Fields::endTime) {
    std::int32_t ticks;
    parse("end time", ticks, begin, end);
    output->endTime = std::chrono::duration<std::int32_t, std::milli>{ticks};
  } else {
    skipAhead("end time", begin, end);
  }

  if (m_fields & LogEntry::Fields::mtime) {
    ninja_clock::rep ticks;
    parse("mtime", ticks, begin, end);
    output->mtime = ninja_clock::time_point{ninja_clock::duration{ticks}};
  } else {
    skipAhead("mtime", begin, end);
  }

  {
    const char* outputStart = begin;
    skipAhead("output", begin, end);
    if (m_fields & LogEntry::Fields::out) {
      output->out = std::string_view{
          outputStart,
          static_cast<std::string_view::size_type>(begin - 1 - outputStart)};
    }
  }

  if (m_fields & LogEntry::Fields::hash) {
    parse("hash", output->hash, begin, end, 16);
  } else {
    skipAhead("hash", begin, end);
  }

  if (begin != end) {
    std::string msg = "Unexpected characters at end of line ";
    msg += std::to_string(m_lineNumber);
    throw std::runtime_error{msg};
  }

  return true;
}

LogReader::iterator LogReader::begin() {
  return iterator{this};
}

LogReader::sentinel LogReader::end() {
  return sentinel{};
}

}  // namespace trimja
