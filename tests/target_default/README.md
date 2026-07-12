# target_default

Exercises `--target-default`: everything listed in the input file's own
`default` statement should be treated as an additional entry point, unioned
with any `--target` values given.

`c` is listed in a `default` statement, `a` is passed explicitly via
`--target`, and `b` depends on `a` but is reachable from neither (e.g. it
packages the project and is never built by CI, and is not part of `default`
either). `bootstrap.ninja` only builds `a` and `c`, so `.ninja_log` never
gets an entry for `b`, matching what would happen on a real CI cache.
