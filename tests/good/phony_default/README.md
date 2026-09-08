# phony_default

We want to make sure that `default` rules with no affected inputs are still
included without modification.  Otherwise we would have to generate a build
command with a `phony` rule to pass to `default` since it cannot exist with
0 inputs.
