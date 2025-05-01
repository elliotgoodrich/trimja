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

#include "murmur_hash.h"

#include <algorithm>

namespace trimja {

std::uint64_t murmur_hash::hash(const void* key, std::size_t length) {
  // The implementation of this has been taken from
  // https://github.com/aappleby/smhasher/blob/61a0530f28277f2e850bfc39600ce61d02b518de/src/MurmurHash2.cpp#L88-L137
  // and was released to the public domain by Austin Appleby.
  // It has been modified not to have any unaligned reads (which is what `ninja`
  // did too), to use a hard-coded seed that is used within `ninja`, and to
  // generally be brought up to modern C++ standards.

  const auto* data = static_cast<const unsigned char*>(key);

  const std::uint64_t m = 0xc6a4'a793'5bd1'e995;
  const int r = 47;

  // This is the seed that is used within `ninja`
  const std::uint64_t seed = 0xdecaf'bad'decaf'bad;
  std::uint64_t h = seed ^ (length * m);

  const std::size_t remainder = length % sizeof(std::uint64_t);
  const unsigned char* end = data + (length - remainder);
  while (data != end) {
    std::uint64_t k;
    std::copy_n(data, sizeof(k), reinterpret_cast<unsigned char*>(&k));
    data += sizeof(k);

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;
  }

  switch (remainder) {  // NOLINT(bugprone-switch-missing-default-case)
    case 7:
      h ^= std::uint64_t{data[6]} << 48;
      [[fallthrough]];
    case 6:
      h ^= std::uint64_t{data[5]} << 40;
      [[fallthrough]];
    case 5:
      h ^= std::uint64_t{data[4]} << 32;
      [[fallthrough]];
    case 4:
      h ^= std::uint64_t{data[3]} << 24;
      [[fallthrough]];
    case 3:
      h ^= std::uint64_t{data[2]} << 16;
      [[fallthrough]];
    case 2:
      h ^= std::uint64_t{data[1]} << 8;
      [[fallthrough]];
    case 1:
      h ^= std::uint64_t{data[0]};
      h *= m;
  };

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;
}

}  // namespace trimja
