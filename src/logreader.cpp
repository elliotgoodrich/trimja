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

#include <array>
#include <cassert>
#include <ranges>

namespace trimja {
namespace {

template <std::size_t N>
std::array<std::string_view, N> splitOnTab(std::string_view in) {
  std::array<std::string_view, N> parts = {};
  auto it = in.begin();
  const auto end = in.end();
  for (std::string_view& part : parts) {
    const auto tab = std::find(it, end, '\t');
    part = std::string_view{it, tab};
    it = tab + std::min<std::size_t>(sizeof('\t'), end - tab);
  }
  return parts;
}

}  // namespace

static_assert(std::input_iterator<LogReader::iterator>);

LogReader::iterator::iterator(LogReader* reader) : m_reader(reader), m_entry() {
  m_reader->read(&m_entry);
}

LogReader::LogReader(std::istream& logs) : m_logs(&logs), m_nextLine() {
  std::getline(*m_logs, m_nextLine, '\n');
  if (m_nextLine != "# ninja log v5") {
    throw std::runtime_error("Unable to find log file signature");
  }
}

bool LogReader::read(LogEntry* output) {
  std::getline(*m_logs, m_nextLine, '\n');
  if (m_nextLine.empty()) {
    return false;
  }

  std::array<std::string_view, 5> parts = splitOnTab<5>(m_nextLine);

  {
    std::int32_t ticks;
    std::from_chars(parts[0].data(), parts[0].data() + parts[0].size(), ticks);
    output->startTime = std::chrono::duration<std::int32_t, std::milli>{ticks};
  }

  {
    std::int32_t ticks;
    std::from_chars(parts[1].data(), parts[1].data() + parts[1].size(), ticks);
    output->endTime = std::chrono::duration<std::int32_t, std::milli>{ticks};
  }

  {
    ninja_clock::rep ticks;
    std::from_chars(parts[2].data(), parts[2].data() + parts[2].size(), ticks);
    output->mtime = std::chrono::clock_cast<std::chrono::file_clock>(
        ninja_clock::time_point{ninja_clock::duration{ticks}});
  }

  output->output = parts[3];
  std::from_chars(parts[4].data(), parts[4].data() + parts[4].size(),
                  output->hash, 16);
  return true;
}

LogReader::iterator LogReader::begin() {
  return iterator(this);
}

LogReader::sentinel LogReader::end() {
  return sentinel();
}

bool operator==(const LogReader::iterator& iter, LogReader::sentinel) {
  return iter.m_reader == nullptr;
}

bool operator!=(const LogReader::iterator& iter, LogReader::sentinel) {
  return iter.m_reader != nullptr;
}

}  // namespace trimja
