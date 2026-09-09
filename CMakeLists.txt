cmake_minimum_required(VERSION 3.26.3)

# Derive the version with the following precedence:
#   1. -DTRIMJA_VERSION_OVERRIDE=<version>  (set by CI from the release tag)
#   2. `git describe` against the most recent `vX.Y.Z` tag
#   3. the fallback "0.0.0" when neither is available
# TRIMJA_VERSION_FULL is the full descriptive version for display, which may
# carry a commit/-dirty suffix such as `1.3.1-5-g1234abcd-dirty`.
# TRIMJA_VERSION_NUMERIC is the plain `X.Y.Z` fed to `project()` (and hence
# CPack).  This runs before `project()` so the result can set the version, and
# is computed at configure time, so a new commit is only reflected after CMake
# re-configures.
if(DEFINED TRIMJA_VERSION_OVERRIDE)
    set(TRIMJA_VERSION_FULL "${TRIMJA_VERSION_OVERRIDE}")
else()
    set(TRIMJA_VERSION_FULL "")
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe
                    --tags --match "v[0-9]*.[0-9]*.[0-9]*" --abbrev=8 --dirty
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE TRIMJA_GIT_DESCRIBE_STATUS
            OUTPUT_VARIABLE TRIMJA_VERSION_FULL
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT TRIMJA_GIT_DESCRIBE_STATUS EQUAL 0)
            set(TRIMJA_VERSION_FULL "")
        endif()
    endif()
endif()

if(TRIMJA_VERSION_FULL STREQUAL "")
    set(TRIMJA_VERSION_FULL "0.0.0")
endif()

# Drop a leading `v` so `v1.3.1` becomes `1.3.1`.
string(REGEX REPLACE "^v" "" TRIMJA_VERSION_FULL "${TRIMJA_VERSION_FULL}")

# `project(VERSION ...)` only accepts numeric components, so pull the leading
# `X.Y.Z` out of the (possibly suffixed) descriptive version.
if(TRIMJA_VERSION_FULL MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(TRIMJA_VERSION_NUMERIC "${CMAKE_MATCH_1}")
else()
    set(TRIMJA_VERSION_NUMERIC "0.0.0")
endif()

message(STATUS "trimja version: ${TRIMJA_VERSION_FULL}")

project(trimja VERSION ${TRIMJA_VERSION_NUMERIC})
enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# Add an option to enable or disable clang-tidy
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy not found. Static analysis will be skipped.")
    endif()
endif()

# Enable ASAN in debug mode for supported compilers
# MSVC is not supported yet as we need to set up the PATH to `clang_rt.asan_dynamic*.dll`
set(ENABLE_SANITIZERS $<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>)

# Decide when to enable fuzzing
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ENABLE_FUZZING 1)
else()
    set(ENABLE_FUZZING 0)
endif()

###############################################################################
# trimja                                                                      #
###############################################################################

add_library(
    core
    STATIC
    src/all.natvis
    src/allocationprofiler.cpp
    src/basicscope.cpp
    src/builddirutil.cpp
    src/cpuprofiler.cpp
    src/depsreader.cpp
    src/depswriter.cpp
    src/edgescope.cpp
    src/evalstring.cpp
    src/fileutil.cpp
    src/graph.cpp
    src/fixed_string.cpp
    src/logreader.cpp
    src/logwriter.cpp
    src/manifestparser.cpp
    src/murmur_hash.cpp
    src/nestedscope.cpp
    src/ninja_clock.cpp
    src/rulevariables.cpp
    src/stringstack.cpp
    src/trimutil.cpp
    thirdparty/gch/small_vector.natvis
    thirdparty/ninja/lexer.cc
    thirdparty/ninja/util.cc
    $<$<BOOL:${WIN32}>:thirdparty/ninja/getopt.c>
)

set_source_files_properties(
    thirdparty/ninja/lexer.cc
    thirdparty/ninja/util.cc
    thirdparty/ninja/getopt.cc
    PROPERTIES SKIP_LINTING ON
)

target_include_directories(core PUBLIC src)
target_include_directories(core SYSTEM PUBLIC thirdparty)
target_compile_options(core PUBLIC
    $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra -Wpedantic>
)
target_compile_definitions(core PUBLIC
    # debug iterators cause default ctor of std::string and std::vector to allocate
    $<$<CXX_COMPILER_ID:MSVC>:_ITERATOR_DEBUG_LEVEL=0 NOMINMAX>
    # Issues with unsupported architecture in MacOS 14
    TINY_OPTIONAL_USE_SEPARATE_BOOL_INSTEAD_OF_UNUSED_BITS
)

# Decide when to enable link-time optimization.
# This is enabled unless we are fuzzing on earlier versions of Clang. e.g. on Clang 12.0.0
# we see:
# /usr/bin/ld: /tmp/lto-llvm-2c8b48.o: warning: sh_link not set for section `__sancov_cntrs'
# /usr/bin/ld: /tmp/lto-llvm-2c8b48.o: warning: sh_link not set for section `__sancov_pcs'
# Note that this is fixed by using fat LTO instead of thin LTO, but CMake only has a
# cross-platform option to enable thin LTO.  This issue is also is fixed in Clang 18.1.3,
# but I'm unsure of the earliest version of Clang that isn't affected.
if(ENABLE_FUZZING)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18.1.3")
        set(ENABLE_LTO OFF)
        message("Info: Link-time optimization has been disabled for this build")
    else()
        set(ENABLE_LTO ON)
    endif()
else()
    set(ENABLE_LTO ON)
endif()

set_property(TARGET core PROPERTY INTERPROCEDURAL_OPTIMIZATION ${ENABLE_LTO})

target_compile_options(
    core PUBLIC
    $<$<AND:${ENABLE_SANITIZERS},$<CXX_COMPILER_ID:MSVC>>:/fsanitize=address>
    $<$<AND:${ENABLE_SANITIZERS},$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-fsanitize=address,undefined>
    $<${ENABLE_FUZZING}:-fsanitize=fuzzer-no-link>
)
target_link_options(
    core PUBLIC
    # MSVC Incremental linking does not work when address sanitizer is enabled
    $<$<AND:${ENABLE_SANITIZERS},$<CXX_COMPILER_ID:MSVC>>:/INCREMENTAL:NO>
    $<$<AND:${ENABLE_SANITIZERS},$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-fsanitize=address,undefined -fno-omit-frame-pointer>
)

add_executable(
    trimja
    src/trimja.m.cpp
)
target_compile_definitions(trimja PRIVATE TRIMJA_VERSION="${TRIMJA_VERSION_FULL}")
set_property(TARGET trimja PROPERTY INTERPROCEDURAL_OPTIMIZATION ${ENABLE_LTO})
target_link_libraries(trimja core)

install(TARGETS trimja RUNTIME DESTINATION bin)

if(WIN32)
    set(CPACK_GENERATOR ZIP NSIS)
else()
    set(CPACK_GENERATOR TGZ)
endif()

set(CPACK_PACKAGE_INSTALL_DIRECTORY trimja)
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
include(CPack)

file(
    GLOB TRIMJA_FUZZING_TARGETS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.fuzz.cpp
)
foreach(FUZZ_TARGET ${TRIMJA_FUZZING_TARGETS})
    if(${FUZZ_TARGET} MATCHES "^(.+)\.fuzz\.cpp$")
        string(CONCAT FUZZ_TARGET_NAME "fuzz" ${CMAKE_MATCH_1})
        add_executable(
            ${FUZZ_TARGET_NAME}
            src/${FUZZ_TARGET}
            $<$<NOT:${ENABLE_FUZZING}>:src/nofuzzing.m.cpp>
        )
        set_property(TARGET ${FUZZ_TARGET_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION ${ENABLE_LTO})
        target_link_options(${FUZZ_TARGET_NAME} PRIVATE $<${ENABLE_FUZZING}:-fsanitize=fuzzer>)
        target_link_libraries(${FUZZ_TARGET_NAME} core)

        # Add tests for the corpora
        if(ENABLE_FUZZING)
            file(
                GLOB TRIMJA_FUZZING_CORPORA
                RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/corpora/${FUZZ_TARGET_NAME}
                ${CMAKE_CURRENT_SOURCE_DIR}/corpora/${FUZZ_TARGET_NAME}/*
            )

            list(LENGTH TRIMJA_FUZZING_CORPORA CORPORA_COUNT)
            if(CORPORA_COUNT GREATER 0)
                add_test(
                    NAME trimja.fuzz.${FUZZ_TARGET_NAME}
                    COMMAND $<TARGET_FILE:${FUZZ_TARGET_NAME}> ${TRIMJA_FUZZING_CORPORA}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/corpora/${FUZZ_TARGET_NAME}/
                    COMMAND_EXPAND_LISTS
                )
            endif()
        else()
            # If fuzzing isn't available, then we build an executable with our own `main` that
            # can iterate through a directory and run the corpora within it
            add_test(
                NAME trimja.fuzz.${FUZZ_TARGET_NAME}
                COMMAND $<TARGET_FILE:${FUZZ_TARGET_NAME}> --directory ${FUZZ_TARGET_NAME}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/corpora/
            )
        endif()
    else()
        message(FATAL_ERROR "Regex should match")
    endif()
endforeach()

# Generate test files for the absolute/relative unit test
get_filename_component(ABSOLUTE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tests/good/absolute/relative" ABSOLUTE)
string(REPLACE ":" "$:" ABSOLUTE_PATH_ESCAPED "${ABSOLUTE_PATH}")
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/tests/good/absolute/changed.txt" "${ABSOLUTE_PATH}\nabsolute")
string(APPEND NINJA_CONTENTS "rule copy\n")
string(APPEND NINJA_CONTENTS "  command = ninja --version \$in -> \$out\n")
string(APPEND NINJA_CONTENTS "build out_relative: copy absolute\n")
string(APPEND NINJA_CONTENTS "build out_absolute: copy ${ABSOLUTE_PATH_ESCAPED}\n")
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/tests/good/absolute/build.ninja" "${NINJA_CONTENTS}")
file(COPY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/tests/good/absolute/build.ninja" "${CMAKE_CURRENT_SOURCE_DIR}/tests/good/absolute/expected.ninja")

# Positive tests: each directory under `tests/good/` is a self-contained ninja
# project that is run through `ninja` and then trimmed, comparing trimja's
# output against its `expected.ninja`.
file(
    GLOB TRIMJA_TESTS
    LIST_DIRECTORIES true
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/*
)

# Sometimes test execution on MSVC leaves behind `_err1_.txt` and `_out1_.txt`
# from previous runs so we filter these out so they don't get mistaken for tests.
list(FILTER TRIMJA_TESTS EXCLUDE REGEX "\.txt$")

# Some tests are Windows-specific due to their use of backslashes as a path separator
if(NOT(WIN32))
    list(FILTER TRIMJA_TESTS EXCLUDE REGEX "win32")
endif()

# Set up test case fixtures
foreach(TEST ${TRIMJA_TESTS})
    # Run ninja over all test directories to generate `.ninja_log` and `.ninja_deps`
    add_test(
        NAME trimja.snapshot.${TEST}.ninja
        COMMAND ninja -d keepdepfile -d keeprsp -f bootstrap.ninja
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/${TEST}/
    )
    set_tests_properties(
        trimja.snapshot.${TEST}.ninja
        PROPERTIES FIXTURES_SETUP trimja.snapshot.${TEST}.fixture
    )
endforeach()

# --version
add_test(NAME trimja.--version COMMAND trimja --version)
string(REPLACE "." "\." TRIMJA_VERSION_REGEX "${TRIMJA_VERSION_FULL}")
set_property(
    TEST trimja.--version
    PROPERTY PASS_REGULAR_EXPRESSION "^${TRIMJA_VERSION_REGEX}\n$"
)

# --help
add_test(NAME trimja.--help COMMAND trimja --help)
set_property(TEST trimja.--help PROPERTY PASS_REGULAR_EXPRESSION "Usage:")
add_test(NAME trimja.-h COMMAND trimja -h)
set_property(TEST trimja.-h PROPERTY PASS_REGULAR_EXPRESSION "Usage:")

# --unknown
add_test(NAME trimja.--unknown COMMAND trimja --unknown)
set_property(TEST trimja.--unknown PROPERTY WILL_FAIL true)
add_test(NAME trimja.-u COMMAND trimja -u)
set_property(TEST trimja.-u PROPERTY WILL_FAIL true)

# No arguments
add_test(NAME trimja.no-args COMMAND trimja)
set_property(TEST trimja.no-args PROPERTY WILL_FAIL true)

# too many arguments
add_test(NAME trimja.--write_and_--output COMMAND trimja --write --output ${CMAKE_CURRENT_BINARY_DIR}/foo.ninja)
set_property(TEST trimja.--write_and_--output PROPERTY WILL_FAIL true)
add_test(NAME trimja.--output_and_--write COMMAND trimja --output ${CMAKE_CURRENT_BINARY_DIR}/foo.ninja --write)
set_property(TEST trimja.--output_and_--write PROPERTY WILL_FAIL true)
add_test(NAME trimja.--affected_and_dash COMMAND trimja --affected changed.txt -)
set_property(TEST trimja.--affected_and_dash PROPERTY WILL_FAIL true)

# Check we can avoid passing `-f`
add_test(
    NAME trimja.snapshot.no-f
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/passthrough/
    COMMAND trimja
    --expected expected.ninja
    --affected changed.txt
)
set_tests_properties(
    trimja.snapshot.no-f
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.passthrough.fixture
)

# Check we can write to the same file we read
add_test(
    NAME trimja.big-in-out
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/bigpassthrough/
    COMMAND trimja --write --affected changed.txt
)
set_tests_properties(
    trimja.big-in-out
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.bigpassthrough.fixture
)

# Check we can write to a different file
add_test(
    NAME trimja.--write
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/passthrough/
    COMMAND trimja --output ${CMAKE_CURRENT_BINARY_DIR}/trimja.ninja --affected changed.txt
)
set_tests_properties(
    trimja.--write
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.passthrough.fixture
)
add_test(
    NAME trimja.--write.cmp
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/passthrough/expected.ninja ${CMAKE_CURRENT_BINARY_DIR}/trimja.ninja
)
set_tests_properties(
    trimja.--write
    PROPERTIES FIXTURES_SETUP trimja.--write.fixture
)
set_tests_properties(
    trimja.--write.cmp
    PROPERTIES FIXTURES_REQUIRED trimja.--write.fixture
)

# Check redirection from a file
if (WIN32)
    add_test(
        NAME trimja.stdin
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/simple/ $ENV{ComSpec} /c $<TARGET_FILE:trimja> - < changed.txt
    )
else()
    add_test(
        NAME trimja.stdin
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/simple/ sh -c "$<TARGET_FILE:trimja> - < changed.txt"
    )
endif()
set_property(TEST trimja.stdin PROPERTY PASS_REGULAR_EXPRESSION "build out1")
set_tests_properties(
    trimja.stdin
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.simple.fixture
)

# --memory-stats
if(WIN32)
    add_test(
        NAME trimja.--memory-stats
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/passthrough/
        COMMAND trimja
        --memory-stats=3
        --affected changed.txt
    )
    set_tests_properties(
        trimja.--memory-stats
        PROPERTIES FIXTURES_REQUIRED trimja.snapshot.passthrough.fixture
    )

    add_test(
        NAME trimja.--memory-stats2
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/builddir/
        COMMAND trimja
        --memory-stats=3
        --builddir
    )
    set_tests_properties(
        trimja.--memory-stats2
        PROPERTIES FIXTURES_REQUIRED trimja.snapshot.builddir.fixture
    )
endif()

# --cpu-stats
add_test(
    NAME trimja.--cpu-stats
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/passthrough/
    COMMAND trimja
    --cpu-stats
    --affected changed.txt
)
set_tests_properties(
    trimja.--cpu-stats
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.passthrough.fixture
)

# --builddir
add_test(
    NAME trimja.--builddir
    COMMAND trimja -f builddir/build.ninja --builddir
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good
)
set_property(TEST trimja.--builddir PROPERTY PASS_REGULAR_EXPRESSION "builddir\/x64\/build")

# Snapshot tests
foreach(TEST ${TRIMJA_TESTS})
    # If a test directory has an `additional_arguments.txt` file, read it (one
    # argument per line, mirroring the one-path-per-line convention already
    # used by `changed.txt`) and pass those extra arguments to `trimja` too.
    # This lets a fixture exercise flags like `--target`/`--target-default`
    # without needing a bespoke `add_test` of its own.
    set(ADDITIONAL_ARGS "")
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/${TEST}/additional_arguments.txt)
        file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tests/good/${TEST}/additional_arguments.txt ADDITIONAL_ARGS)
    endif()

    add_test(
        NAME trimja.snapshot.${TEST}
        COMMAND trimja -f ${TEST}/build.ninja --expected ${TEST}/expected.ninja --affected ${TEST}/changed.txt --explain ${ADDITIONAL_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good
    )
    set_tests_properties(
        trimja.snapshot.${TEST}
        PROPERTIES FIXTURES_REQUIRED trimja.snapshot.${TEST}.fixture
    )

    # Check that --builddir at least returns success on all tests
    add_test(
        NAME trimja.smoke.builddir.${TEST}
        COMMAND trimja -f ${TEST}/build.ninja --builddir
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/good
    )
endforeach()

# Negative tests: each directory under `tests/bad/` describes a trimja
# invocation that must fail.  It contains an `args.txt` (one trimja argument per
# line, mirroring `changed.txt`), an `expected.txt` holding the error message
# trimja must print, and whatever input files the arguments reference.  Unlike
# the positive tests these are not run through `ninja` first: the failure
# happens while parsing the build file, so no fixture is needed.  `expected.txt`
# is used as the test's `PASS_REGULAR_EXPRESSION` (matched against trimja's
# output), the same mechanism used by e.g. `trimja.stdin` above.
file(
    GLOB TRIMJA_BAD_TESTS
    LIST_DIRECTORIES true
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/tests/bad/
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/bad/*
)

foreach(TEST ${TRIMJA_BAD_TESTS})
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tests/bad/${TEST}/args.txt BAD_ARGS)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/tests/bad/${TEST}/expected.txt BAD_EXPECTED)
    string(STRIP "${BAD_EXPECTED}" BAD_EXPECTED)
    add_test(
        NAME trimja.bad.${TEST}
        COMMAND trimja ${BAD_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/bad/${TEST}
    )
    set_property(TEST trimja.bad.${TEST} PROPERTY PASS_REGULAR_EXPRESSION "${BAD_EXPECTED}")
endforeach()

# CMake's built-in `test` target has no dependencies on the binaries it
# exercises (it is not a real target, so `add_dependencies(test ...)` is
# rejected), which means a ninja build graph built from this project never
# shows `test` depending on `trimja` itself. Provide a real custom target
# that depends on every executable used by the test suite so that tools
# consuming build.ninja, such as trimja, see the correct edges.
add_custom_target(
    run_test_with_dependencies
    COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure
    DEPENDS trimja fuzzbuilddirutil fuzzdepsreader fuzzlogreader
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

