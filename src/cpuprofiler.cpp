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

#include "cpuprofiler.h"

#include <list>
#include <ostream>
#include <string>

namespace trimja {

namespace {

std::list<std::pair<std::string, std::chrono::steady_clock::duration>>
    g_cpuMetrics;
bool g_cpuEnabled = false;

}  // namespace

Timer::Timer(std::chrono::steady_clock::duration* output) : m_output{output} {
  if (m_output) {
    m_start = std::chrono::steady_clock::now();
  }
}

Timer::~Timer() {
  stop();
}

void Timer::stop() {  // NOLINT(readability-make-member-function-const)
  if (m_output) {
    *m_output = std::chrono::steady_clock::now() - m_start;
  }
}

void CPUProfiler::enable() {
  g_cpuEnabled = true;
}

bool CPUProfiler::isEnabled() {
  return g_cpuEnabled;
}

Timer CPUProfiler::start(std::string_view name) {
  return Timer{g_cpuEnabled ? &g_cpuMetrics.emplace_back(name, 0).second
                            : nullptr};
}

void CPUProfiler::print(std::ostream& out) {
  for (const auto& [name, duration] : g_cpuMetrics) {
    out << name << ": "
        << std::chrono::duration_cast<std::chrono::microseconds>(duration)
               .count()
        << "us\n";
  }
}

}  // namespace trimja
