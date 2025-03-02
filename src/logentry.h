// MIT License
//
// Copyright (c) 2025 Elliot Goodrich
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

#ifndef TRIMJA_LOGENTRY
#define TRIMJA_LOGENTRY

#include "ninja_clock.h"

#include <chrono>
#include <cstddef>
#include <string_view>

namespace trimja {

/**
 * @enum HashType
 * @brief Represents the type of hash function used to compute the build hash.
 */
enum class HashType {
  murmur,
  rapidhash,
};

/**
 * @struct LogEntry
 * @brief Represents a single log entry.
 *
 * This structure holds information about a single log entry, including
 * start and end times, modification time, output path, and the build hash
 * value.
 */
struct LogEntry {
  /**
   * @enum Fields
   * @brief Represents the fields that can be read from a log entry.
   */
  struct Fields {
    enum {
      startTime = 1 << 1,
      endTime = 1 << 2,
      mtime = 1 << 3,
      out = 1 << 4,
      hash = 1 << 5,
    };
  };

  std::chrono::duration<std::int32_t, std::milli> startTime;
  std::chrono::duration<std::int32_t, std::milli> endTime;
  ninja_clock::time_point mtime;
  std::string_view out;
  std::uint64_t hash;
  HashType hashType;
};

}  // namespace trimja

#endif  // TRIMJA_LOGENTRY
