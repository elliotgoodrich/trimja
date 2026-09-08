# entry_points

Exercises `--target`: only build commands needed for the given target should
survive trimming, with anything unreachable dropped entirely rather than
turned into `phony`.

`b` is a build command that depends on `a` but, unlike `a`, is never built by
CI (e.g. it packages the project). `bootstrap.ninja` only builds `a`, so
`.ninja_log` never gets an entry for `b`, matching what would happen on a
real CI cache: without `--target`, trimja would treat `b` as required simply
because it was never logged, which in turn pulls in `a` too, even though
nothing changed.
