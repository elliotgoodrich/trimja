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

#ifndef TRIMJA_CPUPROFILER
#define TRIMJA_CPUPROFILER

#include <chrono>
#include <iosfwd>
#include <string_view>

namespace trimja {

/**
 * @brief Timer class to measure the duration of code execution.
 */
struct Timer {
  std::chrono::steady_clock::duration* m_output;
  std::chrono::steady_clock::time_point m_start;

  /**
   * @brief Constructs a Timer and optionally starts timing.
   *
   * @param output Optional pointer to a duration object where the elapsed time
   * will be stored if it is not nullptr.
   */
  Timer(std::chrono::steady_clock::duration* output);

  /**
   * @brief Destroys the Timer and stops timing if not already stopped.
   */
  ~Timer();

  /**
   * @brief Stops the timer and writes the elapsed time.
   */
  void stop();
};

/**
 * @brief Utility to enable and manage CPU profiling.
 */
struct CPUProfiler {
  /**
   * @brief Enables the CPU profiler.
   */
  static void enable();

  /**
   * @brief Checks if the CPU profiler is enabled.
   *
   * @return Whether the profiler is enabled.
   */
  static bool isEnabled();

  /**
   * @brief Starts a new timer for profiling a specific code section.
   *
   * @param name The name of the code section being profiled.
   * @return A Timer object that measures the duration of the code section.
   */
  static Timer start(std::string_view name);

  /**
   * @brief Prints the profiling results to the provided output stream.
   *
   * @param out The output stream to print the profiling results.
   */
  static void print(std::ostream& out);
};

}  // namespace trimja

#endif
