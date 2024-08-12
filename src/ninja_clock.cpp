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

namespace std {
namespace chrono {

#if defined(_MSC_VER)

namespace {

// The `file_clock::time_point` in Visual Studio is the same as the `FILETIME`
// struct.
// https://learn.microsoft.com/en-us/cpp/standard-library/file-clock-class

// ninja subtracts the number below to shift the epoch from 1601 to 2001
const std::uint64_t offset = 126'227'704'000'000'000;

}  // namespace

trimja::ninja_clock::time_point
clock_time_conversion<trimja::ninja_clock, file_clock>::operator()(
    file_clock::time_point t) {
  return trimja::ninja_clock::time_point(
      trimja::ninja_clock::duration(t.time_since_epoch().count() - offset));
};

file_clock::time_point
clock_time_conversion<file_clock, trimja::ninja_clock>::operator()(
    trimja::ninja_clock::time_point t) {
  return file_clock::time_point(
      file_clock::duration(t.time_since_epoch().count() + offset));
}
#else
#error "TODO"
#endif

}  // namespace chrono
}  // namespace std
