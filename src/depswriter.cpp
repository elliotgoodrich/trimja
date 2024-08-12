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
#include "ninja_clock.h"

#include <iostream>

namespace trimja {

namespace {

// Ninja's maximum record size, we will try and respect it
const std::size_t NINJA_MAX_RECORD_SIZE = 0b11'1111'1111'1111'1111;

template <typename TYPE>
void writeBinary(std::ostream* out, const TYPE& t) {
  out->write(reinterpret_cast<const char*>(&t), sizeof(t));
}

}  // namespace

DepsWriter::DepsWriter(std::ostream& out) : m_out(&out), m_nextNode(0) {
  const std::string_view signature = "# ninjadeps\n";
  m_out->write(signature.data(), signature.size());
  writeBinary<std::int32_t>(m_out, 4);
}

std::int32_t DepsWriter::recordPath(std::string_view path) {
  const std::uint32_t checksum = ~static_cast<std::uint32_t>(m_nextNode);
  if (path.size() > NINJA_MAX_RECORD_SIZE - sizeof(checksum)) {
    // Substract from `NINJA_MAX_RECORD_SIZE` instead of adding to `paddedSize`
    // since we could end up overflowing
    throw std::runtime_error("Record size exceeded");
  }

  const std::size_t paddedSize = ((path.size() + 3) / 4) * 4;
  writeBinary<std::uint32_t>(m_out, paddedSize + sizeof(checksum));
  m_out->write(path.data(), path.size());
  m_out->write("\0\0", paddedSize - path.size());
  writeBinary<std::uint32_t>(m_out, checksum);
  return m_nextNode++;
}

void DepsWriter::recordDependencies(std::int32_t out,
                                    std::chrono::file_clock::time_point mtime,
                                    std::span<const std::int32_t> nodes) {
  if ((NINJA_MAX_RECORD_SIZE / (nodes.size() + 1)) <
      sizeof(mtime) + sizeof(nodes[0])) {
    throw std::runtime_error("Record size exceeded");
  }

  const std::uint32_t size =
      sizeof(out) + sizeof(mtime) + (nodes.size() * sizeof(nodes[0]));
  // Set the high-bit to indicate a dependency record
  writeBinary<std::uint32_t>(m_out, size | (1u << 31));
  writeBinary<std::int32_t>(m_out, out);
  writeBinary(m_out, std::chrono::clock_cast<ninja_clock>(mtime));
  m_out->write(reinterpret_cast<const char*>(nodes.data()),
               nodes.size() * sizeof(nodes[0]));
}

}  // namespace trimja
