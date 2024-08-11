# trimja

`trimja` is a command line utility to trim down
[ninja](https://ninja-build.org/) to a subset of input files.

This can avoid building unnecessary artifacts when only working on a subset
of the files instead of doing a full build.  If you change a unit test then
it is unnecessary to build and run any of the other unit tests that are
independent.

For example we can improve CI performance by only building those files that
depend on files changed in the pull request.  We can ask `git` to list all
files that differ from the `main` branch and pass `--write` to `trimja` in
order to edit `build.ninja` in place:

```bash
git diff main --name-only | trimja --write
ninja
```

If you just change a README file, we shouldn't have to rebuild the entire
world on the pull request.

In order to handle
[header dependencies](https://ninja-build.org/manual.html#ref_headers) and
[dynamic dependencies](https://ninja-build.org/manual.html#ref_dyndep),
`trimja` will need a `.ninja_deps` file that contains all of these
dependencies.  This is fully generated from a successful `ninja` run.  Any
CI solution using `trimja` will need to cache `.ninja_deps` files from
builds on `main` and load these when running pull requests.

Alternatively, there will be a way to generate `.ninja_deps` files that
will give a great estimate to standard header dependencies.  This is
planned for the `kinja` tool (see below).

## kinja

`kinja` is a command line application to write a ninja dependency file.

In the future `kinja` will be able to generate `.ninja_deps` files for
C/C++ ninja build files.

```bash
kinja -i dependencies.json -o .ninja_deps
kinja -i dependencies.json -f build.ninja
kinja -f build.ninja --generate-from-include-paths
kinja -f build.ninja --generate-from-preprocessor
```
