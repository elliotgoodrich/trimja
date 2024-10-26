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

#ifdef _WIN32
#include <boost/boost_unordered.hpp>

#include <algorithm>
#include <ostream>
#include <span>
#include <stdexcept>
#include <vector>

// <windows.h> must be included before <dbghelp.h>.
#include <windows.h>

#include <dbghelp.h>
#include <intrin.h>
#include <stdio.h>

#pragma comment(lib, "dbghelp.lib")

namespace trimja {

namespace {

struct StackHash {
  using is_transparent = void;
  template <typename C>
  std::size_t operator()(const C& frames) const noexcept {
    std::size_t h = 0;
    for (const void* frame : frames) {
      h ^= std::hash<const void*>{}(frame);
    }
    return h;
  }
};

struct StackEq {
  using is_transparent = void;
  template <typename L, typename R>
  bool operator()(const L& lhs, const R& rhs) const noexcept {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
  }
};

bool s_collect = false;
boost::unordered_flat_map<std::vector<const void*>,
                          std::size_t,
                          StackHash,
                          StackEq>
    s_allocations;
std::vector<const void*> s_tmp(62);

int hook([[maybe_unused]] int allocType,
         [[maybe_unused]] void* userData,
         size_t size,
         [[maybe_unused]] int blockType,
         [[maybe_unused]] long requestNumber,
         [[maybe_unused]] const unsigned char* filename,
         [[maybe_unused]] int lineNumber) {
  if (allocType == _HOOK_ALLOC && size > 0 && s_collect) {
    s_collect = false;

    void* stack[62];
    const USHORT count = CaptureStackBackTrace(0, 62, stack, nullptr);
    // Use a temporary vector as there's no vector constructor taking a span.
    s_tmp.assign(stack, stack + count);
    s_allocations.try_emplace(s_tmp, 0).first->second++;
    s_collect = true;
  }

  return TRUE;
}

}  // namespace

void AllocationProfiler::start() {
  if (SymInitialize(GetCurrentProcess(), NULL, TRUE) == FALSE) {
    throw std::runtime_error("Failed to call SymInitialize.");
  }
  SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
  _CrtSetAllocHook(&hook);
  s_collect = true;
}

void AllocationProfiler::print(std::ostream& out, std::size_t top) {
  s_collect = false;
  std::vector<std::pair<std::span<const void* const>, std::size_t>>
      topAllocations(s_allocations.size());
  std::copy(s_allocations.cbegin(), s_allocations.cend(),
            topAllocations.begin());
  const auto topEnd =
      topAllocations.begin() + std::min(top, s_allocations.size());
  std::partial_sort(
      topAllocations.begin(), topEnd, topAllocations.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

  const HANDLE process = GetCurrentProcess();
  for (auto it = topAllocations.begin(); it != topEnd; ++it) {
    out << it->second << " allocations\n";
    for (const void* frame : it->first) {
      DWORD64 address;
      static_assert(sizeof(frame) == sizeof(address));
      std::copy_n(reinterpret_cast<const char*>(&frame), sizeof(frame),
                  reinterpret_cast<char*>(&address));

      char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
      PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
      symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
      symbol->MaxNameLen = MAX_SYM_NAME;

      if (SymFromAddr(process, address, nullptr, symbol) == TRUE) {
        IMAGEHLP_LINE64 line;
        DWORD displacement;
        if (SymGetLineFromAddr64(process, address, &displacement, &line) ==
            TRUE) {
          out << "  - " << &symbol->Name[0] << " at " << line.FileName << ":"
              << line.LineNumber << "\n";
        }
      }
    }
    out << '\n';
  }
  s_collect = true;
}

}  // namespace trimja
#endif
