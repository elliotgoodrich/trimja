# phony

We want to make sure that the built-in `phony` rule is properly supported.

There are 2 uses of `phony` that we care about:

  1. Referenced `phony`, where there is at least one other non-`phony` build
     command that references the output of the `phony`.  In this case we need to
     include all outputs of that phony command since they are needed by this
     build
  2. Unreferenced/top-level `phony`, where there is a build command use the
     `phony` rule that is not referenced anywhere else by a non-`phony` rule. In
     this case we only include inputs if they are marked required by something
     else.  This use of `phony` is usually to group outputs to give a human a
     nice target.