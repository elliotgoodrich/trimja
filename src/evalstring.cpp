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

#include "evalstring.h"

#include <limits>

// The format of `EvalString` is a sequence of segments. Each segment is
// prefixed with the length of the segment, stored as an `Offset` type.  The
// leading bit of this is set to 1 if the segment is a variable, otherwise it is
// 0 if the segment is text.  Additionally there is an extra member variable
// `m_lastSegmentLength` that has the same value as the last `Offset`.  This
// allows us to jump back to the last segment to extend it if it is a text
// segment.
//
// This has the benefit that `EvalString` if very cache-friendly when iterating
// and requires only one allocation.  Moves and copies should be as cheap as
// possible as well.
//
// The final benefit is that when we call `clear()` we don't free any memory
// meaning that an `EvalString` that is constantly reused will be very
// unlikely to allocate.  Compared to ninja's `EvalString`, which is based on
// `std::vector<std::string>`, which will release memory held in the
// `std::string` objects when clearing the `std::vector`.

namespace trimja {
namespace {

const EvalString::Offset leadingBit =
    static_cast<EvalString::Offset>(1)
    << (std::numeric_limits<EvalString::Offset>::digits - 1);

EvalString::Offset clearLeadingBit(EvalString::Offset in) {
  return in & ~leadingBit;
}

EvalString::Offset setLeadingBit(EvalString::Offset in) {
  return in | leadingBit;
}

bool hasLeadingBit(EvalString::Offset in) {
  return in >> (std::numeric_limits<EvalString::Offset>::digits - 1u);
}

}  // namespace

static_assert(std::forward_iterator<EvalString::const_iterator>);

EvalString::const_iterator::const_iterator() : m_pos{nullptr} {}

EvalString::const_iterator::const_iterator(const char* pos) : m_pos{pos} {}

std::pair<std::string_view, EvalString::TokenType>
EvalString::const_iterator::operator*() const {
  Offset length;
  std::copy_n(m_pos, sizeof(length), reinterpret_cast<char*>(&length));
  return std::make_pair(
      std::string_view{m_pos + sizeof(length), clearLeadingBit(length)},
      hasLeadingBit(length) ? TokenType::Variable : TokenType::Text);
}

EvalString::const_iterator& EvalString::const_iterator::operator++() {
  Offset length;
  std::copy_n(m_pos, sizeof(length), reinterpret_cast<char*>(&length));
  m_pos += sizeof(length) + clearLeadingBit(length);
  return *this;
}

EvalString::const_iterator EvalString::const_iterator::operator++(int) {
  const const_iterator copy = *this;
  ++*this;
  return copy;
}

bool operator==(EvalString::const_iterator lhs,
                EvalString::const_iterator rhs) {
  return lhs.m_pos == rhs.m_pos;
}

bool operator!=(EvalString::const_iterator lhs,
                EvalString::const_iterator rhs) {
  return !(lhs == rhs);
}

EvalString::EvalString() : m_data(sizeof(Offset), '\0'), m_lastSegmentLength{} {
  assert(empty());
}

void EvalString::clear() {
  m_data.assign(sizeof(Offset), '\0');
  m_lastSegmentLength = 0;
  assert(empty());
}

bool EvalString::empty() const {
  return m_data.size() == sizeof(Offset);
}

EvalString::const_iterator EvalString::begin() const {
  return const_iterator{m_data.data()};
}

EvalString::const_iterator EvalString::end() const {
  return const_iterator{m_data.data() + m_data.size()};
}

void EvalString::appendText(std::string_view text) {
  if (!hasLeadingBit(m_lastSegmentLength)) {
    // If the last part was plain text we can extend it
    const Offset newLength = m_lastSegmentLength + text.size();
    std::copy_n(reinterpret_cast<const char*>(&newLength), sizeof(newLength),
                m_data.end() - sizeof(Offset) - m_lastSegmentLength);
    m_data.append(text);
    m_lastSegmentLength = newLength;
  } else {
    // Otherwise write new segment
    m_lastSegmentLength = text.size();
    m_data.append(reinterpret_cast<const char*>(&m_lastSegmentLength),
                  sizeof(m_lastSegmentLength));
    m_data.append(text);
  }
}

void EvalString::appendVariable(std::string_view name) {
  m_lastSegmentLength = setLeadingBit(name.size());
  m_data.append(reinterpret_cast<const char*>(&m_lastSegmentLength),
                sizeof(m_lastSegmentLength));
  m_data.append(name);
}

}  // namespace trimja
