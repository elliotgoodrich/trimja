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

#include "depsreader.h"
#include "depswriter.h"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <ninja/getopt.h>
#else
#include <getopt.h>
#endif

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

namespace {

static const option g_longOptions[] = {
    {"help", no_argument, nullptr, 'h'},
    {"input", required_argument, nullptr, 'i'},
    {"output", required_argument, nullptr, 'o'},
    {"print", required_argument, nullptr, 'p'},
    {"version", no_argument, nullptr, 'v'},
    {},
};

}  // namespace

int main(int argc, char** argv) try {
  using namespace trimja;

  std::optional<std::string> inJSONFile;
  std::optional<std::string> outFile;
  std::optional<std::string> printFile;
  for (int ch = 0; ch != -1;
       ch = getopt_long(argc, argv, "hi:o:p:", g_longOptions, nullptr)) {
    switch (ch) {
      case 0:
        break;
      case 'h':
        std::cout << "Help" << std::endl;
        std::quick_exit(EXIT_SUCCESS);
      case 'i':
        inJSONFile = optarg;
        break;
      case 'o':
        outFile = optarg;
        break;
      case 'p':
        printFile = optarg;
        break;
      case 'v':
        std::cout << KINJA_VERSION << std::endl;
        std::quick_exit(EXIT_SUCCESS);
      case '?':
        std::cout << "Unknown option" << std::endl;
        std::quick_exit(EXIT_FAILURE);
      default:
        std::cout << "Unknown command line parsing error" << std::endl;
        std::quick_exit(EXIT_FAILURE);
    }
  }

  if (printFile) {
    std::ifstream f(*printFile, std::ios::in | std::ios::binary);
    std::ofstream out(*outFile, std::ios::out | std::ios::trunc);
    out << "[\n";

    const char* newLine = "";
    DepsReader reader(f);
    std::vector<std::string> paths;
    while (true) {
      const auto record = reader.read();
      switch (record.index()) {
        case 0: {
          const PathRecordView& view = std::get<PathRecordView>(record);
          out << newLine
              << "    { \"type\": \"addFile\", \"index\": " << view.index
              << ", \"file\": \"" << view.path << "\" }";
          newLine = ",\n";
          paths.emplace_back(view.path);
          break;
        }
        case 1: {
          const DepsRecordView& view = std::get<DepsRecordView>(record);

          // The precision differs between platforms so make sure we truncate or
          // pad "0" to achieve nanosecond precision
          std::string formattedMTime = std::format("{:L%F %T}", view.mtime);
          formattedMTime.resize(
              sizeof("YYYY-MM-DD HH:MM:SS.012345678") - sizeof(""), '0');

          out << newLine << "    { \"type\": \"addDep\", \"out\": \""
              << paths[view.outIndex] << "\", \"datetime\": \""
              << formattedMTime << "\", \"deps\": [";
          newLine = ",\n";
          const char* sep = "";
          for (const std::int32_t dep : view.deps) {
            out << sep << "\"" << paths[dep] << "\"";
            sep = ", ";
          }
          out << "] }";
          break;
        }
        case 2: {
          out << "\n]\n";
          out.flush();
          std::quick_exit(EXIT_SUCCESS);
        }
      }
    }
  } else {
    std::ofstream out(*outFile,
                      std::ios::out | std::ios::binary | std::ios::trunc);
    DepsWriter writer(out);
    std::ifstream f(*inJSONFile);
    nlohmann::json data = nlohmann::json::parse(f);
    std::unordered_map<std::string, std::int32_t> pathLookup;
    for (const nlohmann::json& element : data) {
      if (element["type"] == "addFile") {
        const std::string_view path = element["file"].get<std::string_view>();
        pathLookup.emplace(path, writer.recordPath(path));
      } else if (element["type"] == "addDep") {
        const std::string out = element["out"].get<std::string>();
        const auto outIt = pathLookup.find(out);
        if (outIt == pathLookup.end()) {
          throw std::runtime_error("Missing 'out' property");
        }
        std::vector<std::int32_t> dependencies;
        for (const nlohmann::json& dep : element["deps"]) {
          const auto depIt = pathLookup.find(dep.get<std::string>());
          if (depIt == pathLookup.end()) {
            throw std::runtime_error("Missing 'deps' property");
          }
          dependencies.emplace_back(depIt->second);
        }
        std::istringstream mtimeStream(element["datetime"].get<std::string>());
        std::chrono::file_clock::time_point mtime;
        std::chrono::from_stream(mtimeStream, "%Y-%m-%d %H:%M:%S", mtime);
        writer.recordDependencies(outIt->second, mtime, dependencies);
      } else {
        throw std::runtime_error("Unknown 'type' value");
      }
    }

    out.flush();
    std::quick_exit(EXIT_SUCCESS);
  }
} catch (const std::exception& e) {
  std::cout << e.what() << std::endl;
  std::quick_exit(EXIT_FAILURE);
}