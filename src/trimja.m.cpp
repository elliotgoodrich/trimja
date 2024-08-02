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

#include "trimutil.h"
#ifdef _WIN32
#include <ninja/getopt.h>
#else
#include <getopt.h>
#endif

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace {

static const option g_longOptions[] = {
    {"changed", required_argument, nullptr, 'c'},
    {"expected", required_argument, nullptr, 'e'},
    {"file", required_argument, nullptr, 'f'},
    {"help", no_argument, nullptr, 'h'},
    {"version", no_argument, nullptr, 'v'},
    {},
};

}  // namespace

int main(int argc, char** argv) try {
  using namespace trimja;

  std::at_quick_exit([] { std::cout.flush(); });

  std::string changedFile;
  std::optional<std::string> expectedFile;
  std::string ninjaFile;
  for (int ch = 0; ch != -1;
       ch = getopt_long(argc, argv, "c:e:f:h", g_longOptions, nullptr)) {
    switch (ch) {
      case 0:
        break;
      case 'c':
        changedFile = optarg;
        break;
      case 'e':
        expectedFile = optarg;
        break;
      case 'f':
        ninjaFile = optarg;
        break;
      case 'h':
        std::cout << "Help\n";
        std::quick_exit(EXIT_SUCCESS);
      case 'v':
        std::cout << TRIMJA_VERSION << "\n";
        std::quick_exit(EXIT_SUCCESS);
      case '?':
        std::cout << "Unknown option\n";
        std::quick_exit(EXIT_FAILURE);
      default:
        std::cout << "Unknown command line parsing error\n";
        std::quick_exit(EXIT_FAILURE);
    }
  }

  std::ifstream ninja(ninjaFile);
  std::ifstream changed(changedFile);
  if (!expectedFile) {
    TrimUtil::trim(std::cout, ninjaFile, ninja, changed);
    std::quick_exit(EXIT_SUCCESS);
  }

  std::stringstream trimmed;
  TrimUtil::trim(trimmed, ninjaFile, ninja, changed);
  const std::string_view actual = trimmed.view();

  std::ifstream expected(*expectedFile);
  std::stringstream expectedBuffer;
  expectedBuffer << expected.rdbuf();
  if (actual != expectedBuffer.view()) {
    std::cout << "Output is different to expected\n"
              << "actual:\n"
              << actual << "---\n"
              << "expected:\n"
              << expectedBuffer.view();
    std::quick_exit(EXIT_FAILURE);
  } else {
    std::cout << "Files are equal!\n"
              << "actual:\n"
              << actual << "---\n"
              << "expected:\n"
              << expectedBuffer.view();
    std::quick_exit(EXIT_SUCCESS);
  }
} catch (const std::exception& e) {
  std::cout << e.what();
  std::quick_exit(EXIT_FAILURE);
}