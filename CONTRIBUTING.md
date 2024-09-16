# Contributing

### Prerequisites

  * [CMake](https://cmake.org/)

### Building

  1. 

### Creating a Release

  1. Create a pull request that
     * bumps the version number in `CMakeLists.txt`
     * has `Release vA.B.C` as the commit title (for consistency only)
     * has the release notes for this version in the commit body
  2. Merge the pull request
  3. `git fetch origin`
  4. `git tag vA.B.C origin/main`
  5. `git push origin tag vA.B.C`
`