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

#include "logreader.h"
#include "logwriter.h"

#include <iostream>
#include <sstream>
#include <string>

#include <stdint.h>  // NOLINT(modernize-deprecated-headers)

namespace {

std::string roundTrip(const std::string& data) {
  std::istringstream stream{data};
  trimja::LogReader reader{stream};
  std::ostringstream outStream{std::ios_base::out | std::ios_base::binary};
  trimja::LogWriter writer{outStream, reader.version()};
  for (const trimja::LogEntry& entry : reader) {
    writer.recordEntry(entry);
  }
  return outStream.str();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  try {
    // Round trip twice as the conversion is not bijective, e.g. different
    // strings passed to `std::from_chars` can map to the same integer.
    const std::string input{reinterpret_cast<const char*>(data), size};
    const std::string intermediate = roundTrip(input);
    const std::string output = roundTrip(intermediate);
    if (intermediate != output) {
      std::cout << "Input (size " << input.size() << ")\n"
                << input << "\n---\nIntermediate (size " << intermediate.size()
                << ")\n"
                << intermediate << "\n---\nOutput (size " << output.size()
                << ")\n"
                << output;
      std::abort();
    }
  } catch (const std::exception&) {
  }
  return 0;
}
