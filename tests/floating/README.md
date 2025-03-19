# floating

We want to make sure that rules are always floated to the top (it makes the
algorithm simpler) and that affected (but not required) commands are floated
above non affected commands.

We build `liba`, `libb`, `libc`, `libd` here from several files.  We have
affected files to build `liba`, `libc`, and `libd` and we expect that the
`cc` command for the affected files are at the top, followed by the
corresponding `link` commands for `liba` and `libc`. `libd` stays where it is
because we cannot float past a variable statement without doing dependency
tracking for rules and variables.
