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

#ifndef TRIMJA_ALLOCATIONPROFILER
#define TRIMJA_ALLOCATIONPROFILER

#include <cstddef>
#include <iosfwd>

namespace trimja {

/**
 * @brief The AllocationProfiler class provides functionality to start
 *        and print allocation profiling information.
 *
 * Note that AllocationProfiler will not work in a multi-threaded environment
 * and only has implementation inside Windows.
 */
struct AllocationProfiler {
  /**
   * @brief Starts the allocation profiler.
   *
   * This function initializes the symbol handler and sets the allocation hook
   * to start collecting allocation data.
   *
   * @throws std::runtime_error if SymInitialize fails.
   */
  static void start();

  /**
   * @brief Prints the top allocation data to the provided output stream.
   *
   * This function stops the collection of allocation data, sorts the collected
   * data, and prints the top allocations to the provided output stream.
   *
   * @param out The output stream to print the allocation data.
   * @param top The number of top allocations to print.
   */
  static void print(std::ostream& out, std::size_t top);
};

}  // namespace trimja

#endif