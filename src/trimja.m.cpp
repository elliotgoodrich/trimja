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

#include "allocationprofiler.h"
#include "builddirutil.h"
#include "cpuprofiler.h"
#include "trimutil.h"

#include <ninja/getopt.h>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <variant>

namespace {

const std::string_view g_helpText =
    R"HELP(trimja is a tool to create a smaller ninja build file containing only those
build commands that relate to a specified set of files. This is commonly used
to improve CI performance for pull requests.

trimja requires both the '.ninja_log' and '.ninja_deps' file from a succesful
run of the input ninja build file in order to correctly remove build commands.
Note that with simple ninja input files it is possible for ninja to not
generate either '.ninja_log' or '.ninja_deps', and in this case trimja will
work as expected.

Usage:
$ trimja --version
    Print out the version of trimja ()HELP" TRIMJA_VERSION R"HELP()

$ trimja --help
    Print out this help dialog

$ trimja --builddir [-f FILE]
    Print out the $builddir path in the ninja build file relative to the cwd

$ trimja [-f FILE] [--write | -o OUT] [--affected PATH | -] [--explain]
    Trim down the ninja build file to only required outputs and inputs

Options:
  -f FILE, --file=FILE      path to input ninja build file [default=build.ninja]
  -a PATH, --affected=PATH  path to file containing affected file paths
  -                         read affected file paths from stdin
  -o OUT, --output=OUT      output file path [default=stdout]
  -w, --write               overwrite input ninja build file
  --explain                 print why each part of the build file was kept
  --builddir                print the $builddir variable relative to the cwd)HELP"
#if WIN32
    R"HELP(
  --memory-stats=N          print memory stats and top N allocating functions)HELP"
#endif
    R"HELP(
  --cpu-stats               print timing stats
  -h, --help                print help
  -v, --version             print trimja version ()HELP" TRIMJA_VERSION
    R"HELP()

Examples:

Build only those commands that relate to fibonacci.cpp,
  $ echo "fibonacci.cpp" > changed.txt
  $ trimja --file build.ninja --affected changed.txt --output small.ninja
  $ ninja -f small.ninja

Build only those commands that relate to files that differ from the 'main' git
branch, note the lone '-' argument to specify we are reading from stdin,
  $ git diff main --name-only | trimja - --write
  $ ninja

For more information visit the homepage https://github.com/elliotgoodrich/trimja)HELP";

static const option g_longOptions[] = {
    // TODO: Remove `--expected` and replace with comparing files within CTest
    {"builddir", no_argument, nullptr, 'b'},
    {"explain", no_argument, nullptr, 'e'},
    {"expected", required_argument, nullptr, 'x'},
    {"file", required_argument, nullptr, 'f'},
    {"help", no_argument, nullptr, 'h'},
    {"output", required_argument, nullptr, 'o'},
    {"affected", required_argument, nullptr, 'a'},
    {"version", no_argument, nullptr, 'v'},
    {"write", no_argument, nullptr, 'w'},
    {"memory-stats", required_argument, nullptr, 'm'},
    {"cpu-stats", no_argument, nullptr, 'u'},
    {},
};

std::size_t topAllocatingStacks = 0;
bool instrumentMemory = false;

[[noreturn]] void leave(int rc) {
  if (instrumentMemory) {
    trimja::AllocationProfiler::print(std::cerr, topAllocatingStacks);
    std::cerr.flush();
  }
  if (trimja::CPUProfiler::isEnabled()) {
    trimja::CPUProfiler::print(std::cerr);
    std::cerr.flush();
  }
  std::_Exit(rc);
};

}  // namespace

[[noreturn]] int main(int argc, char* argv[]) try {
  // Decorate as [[noreturn]] to make sure we always call `leave`, which
  // avoids the overhead of destructing objects on the stack.
  using namespace trimja;

  std::ios_base::sync_with_stdio(false);

  struct StdIn {};
  std::variant<std::monostate, StdIn, std::filesystem::path> affectedFile;

  // Remove lone `-` arguments as they are not handled well by `getopt` and
  // treat this as taking affected file paths from stdin.
  //> POSIX Utility Syntax Guidelines
  //>  For utilities that use operands to represent files to be opened for
  //>  either reading or writing, the '-' operand should be used only to mean
  //>  standard input (or standard output when it is clear from context that an
  //>  output file is being specified).
  const auto it = std::remove(argv, argv + argc, std::string_view{"-"});
  if (it != argv + argc) {
    affectedFile.emplace<StdIn>();
    *it = nullptr;  // `argv` must be null-terminated
    argc = static_cast<int>(it - argv);
  }

  struct Stdout {};
  struct Write {};
  struct Expected {};
  std::variant<Stdout, Write, Expected, std::filesystem::path> outputFile;

  std::optional<std::string> expectedFile;
  std::filesystem::path ninjaFile = "build.ninja";
  bool explain = false;
  bool builddir = false;

  int ch;
  while ((ch = getopt_long(argc, argv, "a:f:ho:vw", g_longOptions, nullptr)) !=
         -1) {
    switch (ch) {
      case 'a':
        if (std::get_if<std::monostate>(&affectedFile)) {
          affectedFile.emplace<std::filesystem::path>(optarg);
        } else {
          std::cerr << "Cannot specify --affected when - was given"
                    << std::endl;
          leave(EXIT_FAILURE);
        }
        break;
      case 'b':
        builddir = true;
        break;
      case 'e':
        explain = true;
        break;
      case 'f':
        ninjaFile = optarg;
        break;
      case 'h':
        std::cout << g_helpText << std::endl;
        leave(EXIT_SUCCESS);
      case 'm': {
        const char* last = optarg + std::strlen(optarg);
        auto [ptr, ec] = std::from_chars(optarg, last, topAllocatingStacks);
        if (ec != std::errc{} || ptr != last) {
          std::string msg;
          msg = "'";
          msg += optarg;
          msg += "' is an invalid value for --memory!";
          throw std::runtime_error{msg};
        }
        instrumentMemory = true;
        AllocationProfiler::start();
      } break;
      case 'o':
        if (std::get_if<Stdout>(&outputFile)) {
          outputFile.emplace<std::filesystem::path>(optarg);
        } else if (std::get_if<Write>(&outputFile)) {
          std::cerr << "Cannot specify --output when --write was given"
                    << std::endl;
          leave(EXIT_FAILURE);
        } else if (std::get_if<Expected>(&outputFile)) {
          std::cerr << "Cannot specify --output when --expected was given"
                    << std::endl;
          leave(EXIT_FAILURE);
        } else {
          assert(false);
          leave(EXIT_FAILURE);
        }
        break;
      case 'v':
        std::cout << TRIMJA_VERSION << "" << std::endl;
        leave(EXIT_SUCCESS);
      case 'w':
        if (std::get_if<Stdout>(&outputFile)) {
          outputFile.emplace<Write>();
        } else if (std::get_if<std::filesystem::path>(&outputFile)) {
          std::cerr << "Cannot specify --write when --output was given"
                    << std::endl;
          leave(EXIT_FAILURE);
        } else if (std::get_if<Expected>(&outputFile)) {
          std::cerr << "Cannot specify --write when --expected was given"
                    << std::endl;
          leave(EXIT_FAILURE);
        } else {
          assert(false);
          leave(EXIT_FAILURE);
        }
        break;
      case 'x':
        if (std::get_if<Stdout>(&outputFile)) {
          outputFile.emplace<Expected>();
          expectedFile = optarg;
        } else if (std::get_if<std::filesystem::path>(&outputFile)) {
          std::cerr << "Cannot specify --expected when --output was given"
                    << std::endl;
          leave(EXIT_FAILURE);
        } else if (std::get_if<Write>(&outputFile)) {
          std::cerr << "Cannot specify --expected when --write was given"
                    << std::endl;
          leave(EXIT_FAILURE);
        } else {
          assert(false);
          leave(EXIT_FAILURE);
        }
        break;
      case 'u':
        CPUProfiler::enable();
        break;
      case '?':
        std::cerr << "Unknown option" << std::endl;
        leave(EXIT_FAILURE);
      default:
        std::cerr << "Unknown command line parsing error" << std::endl;
        leave(EXIT_FAILURE);
    }
  }

  // Copy everything from the ninja into a stringstream in case we
  // are overwriting the same input file
  const std::string ninjaFileContents = [&] {
    const Timer ninjaRead = CPUProfiler::start(".ninja read");
    std::stringstream ninjaCopy;
    std::ifstream ninja(ninjaFile);
    ninjaCopy << ninja.rdbuf();
    return ninjaCopy.str();
  }();

  // If we have `--builddir` then ignore all other flags other than -f
  if (builddir) {
    BuildDirUtil util;
    std::cout << util.builddir(ninjaFile, ninjaFileContents).string()
              << std::endl;
    leave(EXIT_SUCCESS);
  }

  std::ifstream affectedFileStream;
  std::istream& affected = std::visit(
      [&](auto&& arg) -> std::istream& {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          std::cerr << "A list of affected files needs to be supplied with "
                       "either --affected [FILE] or - to read from stdin"
                    << std::endl;
          leave(EXIT_FAILURE);
        } else if constexpr (std::is_same_v<T, StdIn>) {
          return std::cin;
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
          affectedFileStream.open(arg);
          return affectedFileStream;
        }
      },
      affectedFile);

  std::variant<std::monostate, std::ofstream, std::stringstream> outStream;
  std::ostream& output = std::visit(
      [&](auto&& arg) -> std::ostream& {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Stdout>) {
          return std::cout;
        } else if constexpr (std::is_same_v<T, Write>) {
          return outStream.emplace<std::ofstream>(ninjaFile);
        } else if constexpr (std::is_same_v<T, Expected>) {
          return outStream.emplace<std::stringstream>();
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
          return outStream.emplace<std::ofstream>(arg);
        }
      },
      outputFile);

  TrimUtil util;
  util.trim(output, ninjaFile, ninjaFileContents, affected, explain);
  output.flush();

  if (!expectedFile.has_value()) {
    leave(EXIT_SUCCESS);
  }

  // Remove CR characters on Windows from the output so that we can cleanly
  // compare the output to the expected file on disk without worrying about
  // different line endings
  std::string actual = std::get<std::stringstream>(outStream).str();
#if _WIN32
  actual.erase(std::remove(actual.begin(), actual.end(), '\r'), actual.end());
#endif

  std::ifstream expectedStream(*expectedFile);
  std::stringstream expectedBuffer;
  expectedBuffer << expectedStream.rdbuf();
  const std::string expected = expectedBuffer.str();
  if (actual != expected) {
    const std::ptrdiff_t pos = std::mismatch(actual.begin(), actual.end(),
                                             expected.begin(), expected.end())
                                   .first -
                               actual.begin();
    std::cout << "Output is different to expected at position " << pos << "\n"
              << "actual (size " << actual.size() << "):\n"
              << actual << "---\n"
              << "expected (size " << expected.size() << "):\n"
              << expected << std::endl;
    leave(EXIT_FAILURE);
  } else {
    std::cout << "Files are equal!\n"
              << "actual:\n"
              << actual << "---\n"
              << "expected:\n"
              << expected << std::endl;
    leave(EXIT_SUCCESS);
  }
} catch (const std::exception& e) {
  std::cout << e.what() << std::endl;
  leave(EXIT_FAILURE);
}
