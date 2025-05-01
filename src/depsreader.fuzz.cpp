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

#include "depsreader.h"
#include "depswriter.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <stdint.h>  // NOLINT(modernize-deprecated-headers)

namespace {

void printHex(const std::string_view str) {
  for (const unsigned char c : str) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(c) << ' ';
  }
  std::cout << '\n';
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  using namespace trimja;
  const std::string input{reinterpret_cast<const char*>(data), size};
  try {
    // TODO: Use std::ispanstream/std::ospanstream in C++23 to avoid string
    // copies
    std::istringstream inStream{input,
                                std::ios_base::in | std::ios_base::binary};
    std::ostringstream outStream{std::ios_base::out | std::ios_base::binary};
    trimja::DepsWriter writer{outStream};
    for (const std::variant<trimja::PathRecordView, trimja::DepsRecordView>&
             record : trimja::DepsReader{inStream}) {
      std::visit(
          [&](auto&& view) {
            using T = std::decay_t<decltype(view)>;
            if constexpr (std::is_same<T, trimja::PathRecordView>()) {
              writer.recordPath(view.path, view.index);
            } else {
              writer.recordDependencies(view.outIndex, view.mtime, view.deps);
            }
          },
          record);
    }
    if (input != outStream.str()) {
      std::cout << "Input = \n";
      printHex(input);
      std::cout << "\n---\nOutput = \n";
      printHex(outStream.str());
      std::abort();
    }
  } catch (const std::exception&) {  // NOLINT(bugprone-empty-catch)
  }
  return 0;
}
