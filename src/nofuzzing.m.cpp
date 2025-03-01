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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <stdint.h>  // NOLINT(modernize-deprecated-headers)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// This entry point runs all of the corpus files in the given directory.  This
// is used on platforms where libFuzzer is not available and it allows us to
// continue testing the corpora.
int main(int argc, const char* argv[]) {
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " --directory [dir]\n";
    return 1;
  }

  if (argv[1] != std::string_view{"--directory"}) {
    std::cout << "Usage: " << argv[0] << " --directory [dir]\n";
    return 1;
  }

  if (!std::filesystem::exists(argv[2]) ||
      !std::filesystem::is_directory(argv[2])) {
    std::cout << "Directory '" << argv[2] << "' does not exist\n";
    return 0;
  }

  std::string buffer;
  for (const std::filesystem::directory_entry& file :
       std::filesystem::directory_iterator{argv[2]}) {
    std::ifstream stream{file.path(), std::ios::binary};
    buffer.assign(std::istreambuf_iterator<char>(stream),
                  std::istreambuf_iterator<char>());

    const int rc = LLVMFuzzerTestOneInput(
        reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
    if (rc != 0) {
      return 1;
    }
  }
}
