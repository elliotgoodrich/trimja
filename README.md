[![CI](https://github.com/elliotgoodrich/trimja/actions/workflows/ci.yaml/badge.svg)](https://github.com/elliotgoodrich/trimja/actions/workflows/ci.yaml)

# trimja

[Ninja](https://ninja-build.org/) is a fast build system commonly used to build
large C/C++ projects.  It does this by looking at a Ninja build file, which
contains the build commands and describes dependencies between files.

**trimja** is a command line utility to trim down Ninja build files to only
those commands that are dependent or necessary for a subset of input files.

This can be used to **speed up CI** to create only those build artifacts that
are affected by the current pull request. For example, a pull request updating
`README.md` shouldn't need to build and test your entire software!

**trimja** will reorder commands within the Ninja build file so build commands
using affected files are prioritized. This means that unsuccessful builds will
fail faster, thereby giving quicker feedback to developers.

For a ready-to-use Github Action see
[trimja-action](https://github.com/elliotgoodrich/trimja-action).

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
  --builddir                print the $builddir variable relative to the cwd
  --memory-stats=N          print memory stats and top N allocating functions
  --cpu-stats               print timing stats
  -h, --help                print help
  -v, --version             print trimja version

Examples:

Build only those commands that relate to fibonacci.cpp,
  $ echo "fibonacci.cpp" > changed.txt
  $ trimja --file build.ninja --affected changed.txt --output small.ninja
  $ ninja -f small.ninja

Build only those commands that relate to files that differ from the 'main' git
branch, note the lone '-' argument to specify we are reading from stdin,
  $ git diff main --name-only | trimja - --write
  $ ninja

For more information visit the homepage https://github.com/elliotgoodrich/trimja
```

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
