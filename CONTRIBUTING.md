# Contributing

## Requirements

The following are prerequsite to building trimja:
  * [CMake](https://cmake.org/)
  * A valid C++20 compiler

The following are optional:
  * [ninja](https://ninja-build.org/) for running unit tests
  * [clang-format](https://clang.llvm.org/docs/ClangFormat.html) for formatting
    code before submitting pull requests
  * [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) for additional
    linting (pass `-DENABLE_CLANG_TIDY=ON` when configuring CMake to enable)

## Building

trimja is a standard CMake project and can be built in a variety of ways, e.g.
Microsoft Visual Studio supports opening CMake projects directly.

A standard workflow to build a Debug version into the `build` folder would look
like:

  1. configure: `cmake -B build -DCMAKE_BUILD_TYPE=Debug -G Ninja`
  2. build: `cmake --build build --config Debug`
  3. test: `ctest --test-dir build --build-config Debug --output-on-failure`
  4. package: `cmake --build build --config Debug --target package`

Note that when using Microsoft Visual Studio these commands need to be run from
the
[developer console](https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell).

## Creating a Release

  1. Create a pull request that
     * bumps the version number in `CMakeLists.txt`
     * has `Release vA.B.C` as the commit title (for consistency only)
     * has the release notes for this version in the commit body
  2. Merge the pull request
  3. `git fetch origin`
  4. `git tag vA.B.C origin/main`
  5. `git push origin tag vA.B.C`
`