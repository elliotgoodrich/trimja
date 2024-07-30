# trimja

## Introduction

`trimja` is a command line utility to trim down
[ninja](https://ninja-build.org/) to a subset of input files.

This can avoid building unnecessary artifacts when only working on a subset
of the files instead of doing a full build.  If you change a unit test then
it is unnecessary to build and run any of the other unit tests that are
independent.

```bash
trimja -f build.ninja foo.cpp > trim.ninja
ninja trim.ninja
```

For example we can improve CI performance by only building those files that
depend on files changed in the pull request.  The `-` command line option
tells `trimja` to take the changed files from stdin, each separated by
a newline:

```bash
git diff main --name-only | trimja -f build.ninja - > trim.ninja
ninja trim.ninja
```

If you just change a README file, we shouldn't have to rebuild the entire
world on the pull request.

Like `ninja`, `trimja` will look for the `build.ninja` file in the current
working directory so we can omit passing `-f build.ninja` in these cases,

```bash
git diff main --name-only | trimja - > trim.ninja
```