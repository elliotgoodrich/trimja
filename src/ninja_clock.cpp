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

#include "ninja_clock.h"

namespace trimja {

namespace {

#if defined(_MSC_VER)
// The `file_clock::time_point` in Visual Studio is the same as the `FILETIME`
// struct.
// https://learn.microsoft.com/en-us/cpp/standard-library/file-clock-class
// ninja subtracts the number below to shift the epoch from 1601 to 2001
const std::uint64_t offset = 126'227'704'000'000'000;
#else
// For other platforms (we're assuming POSIX) the `file_clock::time_point` is
// nanoseconds past the unix epoch and ninja doesn't adjust this at all. There's
// some complications such as libc++ using __int128_t
// (https://libcxx.llvm.org/DesignDocs/FileTimeType.html) but we will truncate
// in the exact same way that ninja will
const std::uint64_t offset = 0;
#endif

}  // namespace

trimja::ninja_clock::time_point ninja_clock::from_file_clock(
    std::chrono::file_clock::time_point t) {
  return trimja::ninja_clock::time_point(
      trimja::ninja_clock::duration(t.time_since_epoch().count() - offset));
}

std::chrono::file_clock::time_point ninja_clock::to_file_clock(
    trimja::ninja_clock::time_point t) {
  return std::chrono::file_clock::time_point(
      std::chrono::file_clock::duration(t.time_since_epoch().count() + offset));
}

}  // namespace trimja

#if 0
namespace std {
namespace chrono {

trimja::ninja_clock::time_point
clock_time_conversion<trimja::ninja_clock, file_clock>::operator()(
    file_clock::time_point t) {
  return trimja::ninja_clock::time_point(
      trimja::ninja_clock::duration(t.time_since_epoch().count() - trimja::offset));
};

file_clock::time_point
clock_time_conversion<file_clock, trimja::ninja_clock>::operator()(
    trimja::ninja_clock::time_point t) {
  return file_clock::time_point(
      file_clock::duration(t.time_since_epoch().count() + trimja::offset));
}

}  // namespace chrono
}  // namespace std
#endif
