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

#include "depswriter.h"

#include <cassert>
#include <ostream>

namespace trimja {

namespace {

// Ninja's maximum record size, we will try and respect it
const std::size_t NINJA_MAX_RECORD_SIZE = 0b11'1111'1111'1111'1111;

void write(std::ostream* out, const void* data, std::size_t size) {
  static_assert(
      NINJA_MAX_RECORD_SIZE <= std::numeric_limits<std::streamsize>::max(),
      "Narrowing cast could lose information");
  assert(size <= NINJA_MAX_RECORD_SIZE);
  out->write(static_cast<const char*>(data),
             static_cast<std::streamsize>(size));
}

template <typename TYPE>
void writeBinary(std::ostream* out, const TYPE& t) {
  write(out, &t, sizeof(t));
}

}  // namespace

DepsWriter::DepsWriter(std::ostream& out) : m_out{&out}, m_nextNode{0} {
  const std::string_view signature = "# ninjadeps\n";
  write(m_out, signature.data(), signature.size());
  writeBinary<std::int32_t>(m_out, 4);
}

std::int32_t DepsWriter::recordPath(std::string_view path) {
  const std::int32_t nodeId = recordPath(path, m_nextNode);
  ++m_nextNode;
  return nodeId;
}

std::int32_t DepsWriter::recordPath(std::string_view path,
                                    std::int32_t nodeId) {
  const std::uint32_t checksum = ~static_cast<std::uint32_t>(nodeId);
  if (path.size() > NINJA_MAX_RECORD_SIZE - sizeof(checksum)) {
    // Substract from `NINJA_MAX_RECORD_SIZE` instead of adding to `paddedSize`
    // since we could end up overflowing
    throw std::runtime_error{"Record size exceeded"};
  }

  const std::size_t paddedSize = ((path.size() + 3) / 4) * 4;
  writeBinary(m_out, static_cast<std::uint32_t>(paddedSize + sizeof(checksum)));
  write(m_out, path.data(), path.size());
  assert(paddedSize - path.size() <= sizeof("\0\0"));
  write(m_out, "\0\0", paddedSize - path.size());
  writeBinary<std::uint32_t>(m_out, checksum);
  return m_nextNode++;
}

void DepsWriter::recordDependencies(
    std::int32_t out,
    ninja_clock::time_point mtime,
    std::span<const std::int32_t> dependencies) {
  if ((NINJA_MAX_RECORD_SIZE / (dependencies.size() + 1)) <
      sizeof(mtime) + sizeof(dependencies[0])) {
    throw std::runtime_error{"Record size exceeded"};
  }

  const auto size = static_cast<std::uint32_t>(
      sizeof(out) + sizeof(mtime) +
      (dependencies.size() * sizeof(dependencies[0])));
  // Set the high-bit to indicate a dependency record
  writeBinary<std::uint32_t>(m_out, size | (1U << 31));
  writeBinary<std::int32_t>(m_out, out);
  writeBinary(m_out, mtime);
  write(m_out, dependencies.data(),
        dependencies.size() * sizeof(dependencies[0]));
}

}  // namespace trimja
