# trimja

`trimja` is a command line utility to trim down
[ninja](https://ninja-build.org/) to a subset of input files.

This can avoid building unnecessary artifacts when only working on a subset
of the files instead of doing a full build.  If you change a unit test then
it is unnecessary to build and run any of the other unit tests that are
independent.

For example we can improve CI performance by only building those files that
depend on files changed in the pull request.  We can ask `git` to list all
files that differ from the `main` branch, pass `--write` to `trimja` in
order to edit `build.ninja` in place, and pass `-` to take a list of affected
file from stdin:

```bash
git diff main --name-only | trimja - --write
ninja
```

If you just change a README file, we shouldn't have to rebuild the entire
world on the pull request.

In order to handle
[header dependencies](https://ninja-build.org/manual.html#ref_headers) and
[dynamic dependencies](https://ninja-build.org/manual.html#ref_dyndep),
`trimja` will need a `.ninja_deps` file that contains all of these
dependencies.  This is fully generated from a successful `ninja` run.  In
addition we will also need the `.ninja_log` file to determine if any build
commands have been edited. Any CI solution using `trimja` will need to cache
`.ninja_deps` files from builds on `main` and load these when running pull
requests.

