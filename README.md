[![CI](https://github.com/elliotgoodrich/trimja/actions/workflows/ci.yaml/badge.svg)](https://github.com/elliotgoodrich/trimja/actions/workflows/ci.yaml)

# trimja

[Ninja](https://ninja-build.org/) is a fast build system commonly used to build
large C/C++ projects.  It does this by looking at a Ninja build file, which
contains the build commands and describes dependencies between files.

`trimja` is a command line utility to trim down Ninja build files to only
those commands that are dependent or necessary for a subset of input files.

This can be used to **speed up CI** to create only those build artifacts that
are affected by the current pull request. For example, a pull request updating
`README.md` shouldn't need to build and test your entire software!

**trimja** will reorder commands within the Ninja build file so build commands
using affected files are prioritized. This means that unsuccessful builds will
fail faster, thereby giving quicker feedback to developers.

For a ready-to-use Github Action see
[trimja-action](https://github.com/elliotgoodrich/trimja-action).  This
repository is using `trimja` itself on pull requests via `trimja-action` to
avoid unnecessary work.

## Install

  1. Download the package for your platform from the
     [releases page](https://github.com/elliotgoodrich/trimja/releases) and
     install it.  On Windows run the `.exe` installer or unzip the `.zip`; on
     Linux and macOS extract the `.tar.gz`.
  2. Make sure the `trimja` executable is on your `PATH`.  The Windows installer
     can do this for you; otherwise add the extracted `bin` directory to your
     `PATH`.  Check it works by running `trimja --version`.
  3. (Optional) Verify the download's build provenance with the
     [GitHub CLI](https://cli.github.com/):
     ```
     gh attestation verify <downloaded-file> --repo elliotgoodrich/trimja
     ```

## Help

The following instruction on how to use trimja can be found by running
`trimja --help` or `trimja -h`.

```
trimja is a tool to create a smaller ninja build file containing only those
build commands that relate to a specified set of files. This is commonly used
to improve CI performance for pull requests.

trimja requires both the '.ninja_log' and '.ninja_deps' file from a succesful
run of the input ninja build file in order to correctly remove build commands.
Note that with simple ninja input files it is possible for ninja to not
generate either '.ninja_log' or '.ninja_deps', and in this case trimja will
work as expected.

Usage:
$ trimja --version
    Print out the version of trimja

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
  --no-summary              do not print out a summary
  --builddir                print the $builddir variable relative to the cwd
  --memory-stats=N          print memory stats and top N allocating functions
  --cpu-stats               print timing stats
  -h, --help                print help
  -v, --version             print trimja version

Examples:

Build only those commands that relate to fibonacci.cpp.  trimja prints to stderr
roughly how much build time it saved (pass --no-summary to silence this),
  $ echo "fibonacci.cpp" > changed.txt
  $ trimja --file build.ninja --affected changed.txt --output small.ninja
  trimja removed roughly 10m 30s of build items from build.ninja (~42%)
  $ ninja -f small.ninja

Build only those commands that relate to files that differ from the 'main' git
branch, note the lone '-' argument to specify we are reading from stdin,
  $ git diff main --name-only | trimja - --write
  trimja removed roughly 6m 41s of build items from build.ninja (~20%)
  $ ninja

For more information visit the homepage https://github.com/elliotgoodrich/trimja
```

## Common Pitfalls

trimja only knows what the Ninja build file, `.ninja_log`, `.ninja_deps` and
your list of affected files tell it.  A build command is kept if one of its
inputs is affected, if it is missing from `.ninja_log`, or if its command line
differs from the one recorded there.  Anything that changes without showing up
in the affected files or a command line will not cause a rebuild, and anything
that isn't rebuilt won't exist on a fresh CI machine.  The examples below use
CMake, but the same ideas apply to any generator.

### Dependencies downloaded to a fixed directory

If a third-party dependency is downloaded at configure time (e.g. with CMake's
`FetchContent`) into the same directory whatever its version, then bumping the
version only changes your `CMakeLists.txt`.  The headers and sources at that
path change, but they aren't in your `git diff` and the compile commands using
them are identical, so nothing that uses the dependency is rebuilt.

Put the version, or better its hash, in the directory name so that a new
version changes the include paths, and therefore the command lines, of
everything that uses it,

```cmake
FetchContent_Declare(
  foo
  URL https://example.com/foo-1.2.3.tar.gz
  URL_HASH SHA256=0123456789abcdef...
  SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/foo-0123456789ab"
)
```

Prefer downloading a hashed archive to a git tag, as a tag can be moved.

### Compiler and system library updates

A CI runner image can update the compiler or a system library without its path
changing, and those files are outside of your repository so they never appear
in your `git diff`.  To rebuild everything when the compiler changes, add an
unused definition to every compile command,

```cmake
string(MAKE_C_IDENTIFIER "${CMAKE_CXX_COMPILER_ID}_${CMAKE_CXX_COMPILER_VERSION}" toolchain)
add_compile_definitions("TRIMJA_TOOLCHAIN=${toolchain}")
```

Do this straight after `project()` so it also reaches your dependencies.  Do the
same for a system library you build against, using the version `pkg-config`
reports, rather than any "API version" macro your code already defines, as that
stays the same when the library is updated.  Keep the value a plain identifier,
as a value like `Clang-19.1.1` looks like an expression and is rejected by
clang-tidy's `bugprone-macro-parentheses` check.

### Running tests whose binaries weren't built

After trimming, the binaries of unaffected tests won't exist, and most test
runners treat a missing binary as a failure.  For example, CTest runs a
placeholder `<target>_NOT_BUILT` test that always fails for each target passed
to `gtest_discover_tests` that hasn't been built, and a test script given the
path of a missing binary will fail too.  Skipping these is correct, as nothing
they depend on has changed,

  * For `gtest_discover_tests`, include a file after GoogleTest's own that
    disables the placeholder.  Disabled tests are reported but don't fail
    CTest, and the property is ignored once the binary is built and the
    placeholder no longer exists,

    ```cmake
    gtest_discover_tests(foo_test)
    file(CONFIGURE OUTPUT foo_test_skip.cmake CONTENT
      "set_tests_properties(foo_test_NOT_BUILT PROPERTIES DISABLED TRUE)\n")
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
      "${CMAKE_CURRENT_BINARY_DIR}/foo_test_skip.cmake")
    ```

    This has to be an include file, as CTest reads a directory's include files
    before the tests added with `add_test`.
  * For a test run by a script, have the script check the binaries it was
    given exist and exit with a distinct code, e.g. 77, when one doesn't, then
    set `SKIP_RETURN_CODE 77` on the test.  A CMake `-P` script can't choose its
    exit code before CMake 3.29, so print a marker instead and set
    `SKIP_REGULAR_EXPRESSION`, which skips the test even if the script fails.

Put these behind an option that only your trimmed CI builds turn on, so that a
test binary someone forgot to build locally still fails.

### Inputs that only tests and programs read

Test scripts, test data and anything loaded at runtime, such as a plugin or a
DLL, are often not inputs to any build command.  A change to only those files
then marks nothing as affected, the test's binaries aren't built, and the test
is skipped when it should run.

To make a DLL an input of the binaries that load it, add it to their link step,

```cmake
set_property(TARGET foo_test APPEND PROPERTY LINK_DEPENDS $<TARGET_FILE:my_plugin>)
```

For a test that needs several binaries and some files, add a custom command
that depends on all of them, and build it by default.  If any of those inputs
are affected, so is its output, and trimja keeps every input it needs, so all
of the binaries are built together and the test runs,

```cmake
add_custom_command(OUTPUT foo.stamp
  COMMAND "${CMAKE_COMMAND}" -E touch foo.stamp
  DEPENDS foo foo_helper "${CMAKE_CURRENT_SOURCE_DIR}/foo_test.sh"
  VERBATIM)
add_custom_target(foo_inputs ALL DEPENDS foo.stamp)
```

This has to be a real command rather than a `phony` target, as an affected
`phony` doesn't make trimja keep its inputs.

### Configuration files read by tools

A tool can read configuration that isn't an input of the command running it,
such as `clang-tidy` reading `.clang-tidy` when CMake runs it as part of each
compile.  Either list that file as an affected file whenever it changes, or
don't trim the jobs that use it.

## CI Design

Integrating trimja into a CI pipeline requires an external cache where the
`.ninja_log` and `.ninja_deps` files can be stored for successful builds.  The
`.ninja_log` file contains a list of all files built with the hash of the build
command and the `.ninja_deps` file contains dynamic dependencies generated
during the build - such as includes header files when compiling C/C++ source
files.

  1. On requiring to build a new commit (e.g. on a PR or on merging to `main`)
  2. Configure your build and generate the `ninja.build` file
  3. Attempt to find the cache entry for the most recent ancestor commit of
     `HEAD`
  4. If a cache entry is found, move to the next step, otherwise skip to step 7
  5. Note the commit hash that generated this cache hit (call this `EXISTING`)
     and download the cache to the correct location
  6. Run `git diff EXISTING..HEAD --name-only | trimja - --write` to replace the
     `build.ninja` file
  7. Run `ninja`
  8. If the build is successful upload `.ninja_log` and `.ninja_deps` to the
     cache
