# `time-saved`: trimja prints, on stderr, roughly how much build time it trimmed
# away, computed from the durations recorded in the previous `.ninja_log`.
#
# The committed `.ninja_log` here is hand-crafted: its output paths and command
# hashes are genuine (so trimja treats both commands as unchanged), but the
# start/end times are fabricated to give nice round durations.  `slow.c` is not
# affected so `slow.o` (1h 20m) is phony-ed out, while `kept.c` is affected so
# `kept.o` (20m) is kept -- 1h 20m of a 1h 40m build, i.e. ~80%, is removed.
#
# Each output also has an earlier duplicate entry with different timings; ninja
# appends a fresh line on every rebuild and the last entry wins, so this checks
# that we use only the final entry per output (the earlier ones would give a
# different duration and percentage).
#
# To regenerate the hashes after editing `build.ninja`, run `ninja -f
# build.ninja` in this directory and then re-fabricate the time columns.
add_test(
    NAME trimja.ugly.time-saved
    COMMAND trimja -f build.ninja --affected changed.txt
            --output ${CMAKE_CURRENT_BINARY_DIR}/out.ninja
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_property(
    TEST trimja.ugly.time-saved
    PROPERTY PASS_REGULAR_EXPRESSION
    "trimja removed roughly 1h 20m of build items from build\\.ninja \\(~80%\\)"
)

# Test `--no-summary`
add_test(
    NAME trimja.ugly.time-saved.no-summary
    COMMAND trimja -f build.ninja --affected changed.txt --no-summary
            --output ${CMAKE_CURRENT_BINARY_DIR}/out-no-summary.ninja
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_property(
    TEST trimja.ugly.time-saved.no-summary
    PROPERTY FAIL_REGULAR_EXPRESSION "trimja removed roughly"
)

# When nothing can be trimmed (here both sources are affected, so both commands
# are kept) trimja still reports that it removed nothing rather than staying
# silent or printing "roughly 0s".
add_test(
    NAME trimja.ugly.time-saved.nothing-removed
    COMMAND trimja -f build.ninja --affected changed-all.txt
            --output ${CMAKE_CURRENT_BINARY_DIR}/out-nothing-removed.ninja
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_property(
    TEST trimja.ugly.time-saved.nothing-removed
    PROPERTY PASS_REGULAR_EXPRESSION
    "trimja could not remove any build items from build\\.ninja"
)
