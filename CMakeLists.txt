cmake_minimum_required(VERSION 3.26.3)
project(trimja VERSION 0.0.1)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

###############################################################################
# trimja                                                                      #
###############################################################################

add_executable(
    trimja
    src/trimja.m.cpp
    src/depsreader.cpp
    src/graph.cpp
    src/ninja_clock.cpp
    src/trimutil.cpp
    thirdparty/ninja/lexer.cc
    $<$<BOOL:${WIN32}>:thirdparty/ninja/getopt.c>
)
target_compile_definitions(trimja PRIVATE TRIMJA_VERSION="${CMAKE_PROJECT_VERSION}")
target_include_directories(trimja PRIVATE thirdparty)

# --version
add_test(NAME trimja.--version COMMAND trimja --version)
string(REPLACE "." "\." TRIMJA_VERSION_REGEX ${CMAKE_PROJECT_VERSION})
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

# too many arguments
add_test(NAME trimja.--write_and_--output COMMAND trimja --write --output ${CMAKE_CURRENT_BINARY_DIR}/foo.ninja)
set_property(TEST trimja.--write_and_--output PROPERTY WILL_FAIL true)
add_test(NAME trimja.--output_and_--write COMMAND trimja --output ${CMAKE_CURRENT_BINARY_DIR}/foo.ninja --write)
set_property(TEST trimja.--output_and_--write PROPERTY WILL_FAIL true)

# Check we can avoid passing `-f`
add_test(
    NAME trimja.snapshot.no-f
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/passthrough/
    COMMAND trimja
    --expected expected.ninja
    --restrict changed.txt
)

# Check we can write to the same file we read
add_test(
    NAME trimja.big-in-out
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/bigpassthrough/
    COMMAND trimja --write --restrict changed.txt
)

# Check we can write to a different file
add_test(
    NAME trimja.--write
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/passthrough/
    COMMAND trimja --output ${CMAKE_CURRENT_BINARY_DIR}/trimja.ninja --restrict changed.txt
)
add_test(
    NAME trimja.--write.cmp
    COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/passthrough/build.ninja ${CMAKE_CURRENT_BINARY_DIR}/trimja.ninja
)
set_tests_properties(
    trimja.--write.cmp
    PROPERTIES FIXTURES_REQUIRED trimja.--write.fixture
)
set_tests_properties(
    trimja.--write
    PROPERTIES FIXTURES_SETUP trimja.--write.fixture
)

# Check redirection from a file
if (WIN32)
    add_test(
        NAME trimja.stdin
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/simple/ $ENV{ComSpec} /c $<TARGET_FILE:trimja> < changed.txt
    )
else()
    add_test(
        NAME trimja.stdin
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/simple/ sh -c "$<TARGET_FILE:trimja> < changed.txt"
    )
endif()
set_property(TEST trimja.stdin PROPERTY PASS_REGULAR_EXPRESSION "build out1")

# Snapshot tests
foreach(TEST default passthrough simple simple_reversed chained fan pyramid dependencies attached basic_dyndep)
    add_test(
        NAME trimja.snapshot.${TEST}
        COMMAND trimja
        -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/${TEST}/build.ninja
        --expected ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/${TEST}/expected.ninja
        --restrict ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/${TEST}/changed.txt
    )
endforeach()

###############################################################################
# kinja                                                                       #
###############################################################################

add_executable(
    kinja
    src/kinja.m.cpp
    src/depsreader.cpp
    src/depswriter.cpp
    src/graph.cpp
    src/ninja_clock.cpp
    src/trimutil.cpp
    thirdparty/ninja/lexer.cc
    $<$<BOOL:${WIN32}>:thirdparty/ninja/getopt.c>
)
target_compile_definitions(kinja PRIVATE KINJA_VERSION="${CMAKE_PROJECT_VERSION}")
target_include_directories(kinja PRIVATE thirdparty)
target_compile_definitions(kinja PRIVATE JSON_DIAGNOSTICS=1)

# --version
add_test(NAME kinja.--version COMMAND kinja --version)
string(REPLACE "." "\." KINJA_VERSION_REGEX ${CMAKE_PROJECT_VERSION})
set_property(
    TEST kinja.--version
    PROPERTY PASS_REGULAR_EXPRESSION "^${KINJA_VERSION_REGEX}\n$"
)

# --help
add_test(NAME kinja.--help COMMAND kinja --help)
set_property(TEST kinja.--help PROPERTY PASS_REGULAR_EXPRESSION "Help")
add_test(NAME kinja.-h COMMAND kinja -h)
set_property(TEST kinja.-h PROPERTY PASS_REGULAR_EXPRESSION "Help")

# --unknown
add_test(NAME kinja.--unknown COMMAND kinja --unknown)
set_property(TEST kinja.--unknown PROPERTY WILL_FAIL true)
add_test(NAME kinja.-u COMMAND kinja -u)
set_property(TEST kinja.-u PROPERTY WILL_FAIL true)

# Snapshot tests
foreach(TEST basic)
    set(NINJA_JSON_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/expected.crlf.json)
    set(NINJA_JSON_ACTUAL ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/actual.json)
    set(NINJA_JSON_EXPECTED ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/expected.$<IF:$<BOOL:${WIN32}>,crlf,lf>.json)
    set(NINJA_DEPS_ACTUAL ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/actual.dat)
    # .ninja_deps files differ based on platform so our expected file is different
    set(NINJA_DEPS_EXPECTED ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/expected.$<IF:$<BOOL:${WIN32}>,win,posix>.dat)

    add_test(
        NAME kinja.snapshot.${TEST}
        COMMAND kinja --input ${NINJA_JSON_INPUT} --output ${NINJA_DEPS_ACTUAL}
    )
    add_test(
        NAME kinja.snapshot.${TEST}.cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files ${NINJA_DEPS_ACTUAL} ${NINJA_DEPS_EXPECTED}
    )
    set_tests_properties(
        kinja.snapshot.${TEST}.cmp
        PROPERTIES FIXTURES_REQUIRED kinja.snapshot.prettyprint.${TEST}.fixture
    )
    set_tests_properties(kinja.snapshot.${TEST}
        PROPERTIES FIXTURES_SETUP kinja.snapshot.${TEST}.fixture
    )

    add_test(
        NAME kinja.snapshot.prettyprint.${TEST}
        COMMAND kinja --print ${NINJA_DEPS_EXPECTED} --output ${NINJA_JSON_ACTUAL}
    )
    add_test(
        NAME kinja.snapshot.prettyprint.${TEST}.cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files ${NINJA_JSON_ACTUAL} ${NINJA_JSON_EXPECTED}
    )
    set_tests_properties(
        kinja.snapshot.prettyprint.${TEST}.cmp
        PROPERTIES FIXTURES_REQUIRED kinja.snapshot.prettyprint.${TEST}.fixture
    )
    set_tests_properties(kinja.snapshot.prettyprint.${TEST}
        PROPERTIES FIXTURES_SETUP kinja.snapshot.prettyprint.${TEST}.fixture
    )

endforeach()

# TODO: Test kinja with bad JSON
