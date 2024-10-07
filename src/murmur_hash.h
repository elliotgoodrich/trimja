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

#ifndef TRIMJA_MURMUR_HASH
#define TRIMJA_MURMUR_HASH

#include <cstddef>
#include <cstdint>

namespace trimja {

/**
 * @struct murmur_hash
 * @brief Provides a static method to compute the MurmurHash.
 */
struct murmur_hash {
  /**
   * @brief Computes the MurmurHash for the given key.
   *
   * @param key Pointer to the data to hash.
   * @param length Length of the data in bytes.
   * @return 64-bit hash value.
   */
  static std::uint64_t hash(const void* key, std::size_t length);
};

}  // namespace trimja

#endif  // TRIMJA_TRIMUTIL
