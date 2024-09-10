cmake_minimum_required(VERSION 3.26.3)
project(trimja VERSION 0.0.1)
enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

###############################################################################
# trimja                                                                      #
###############################################################################

add_executable(
    trimja
    src/trimja.m.cpp
    src/depsreader.cpp
    src/graph.cpp
    src/logreader.cpp
    src/murmur_hash.cpp
    src/ninja_clock.cpp
    src/parser.cpp
    src/trimutil.cpp
    thirdparty/ninja/eval_env.cc
    thirdparty/ninja/lexer.cc
    thirdparty/ninja/util.cc
    $<$<BOOL:${WIN32}>:thirdparty/ninja/getopt.c>
)
target_compile_definitions(trimja PRIVATE TRIMJA_VERSION="${CMAKE_PROJECT_VERSION}")
target_include_directories(trimja PRIVATE thirdparty)
target_compile_options(trimja PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

file(
    GLOB TRIMJA_TESTS
    LIST_DIRECTORIES true
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/tests/
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*
)

# Some tests are Windows-specific due to their use of backslashes as a path separator
if(NOT(WIN32))
    list(FILTER TRIMJA_TESTS EXCLUDE REGEX "win32")
endif()

# Set up test case fixtures
foreach(TEST ${TRIMJA_TESTS})
    # Run ninja over all test directories to generate `.ninja_log` and `.ninja_deps`
    add_test(
        NAME trimja.snapshot.${TEST}.ninja
        COMMAND ninja -d keepdepfile -d keeprsp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST}/
    )
    set_tests_properties(
        trimja.snapshot.${TEST}.ninja
        PROPERTIES FIXTURES_SETUP trimja.snapshot.${TEST}.fixture
    )
endforeach()

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
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/passthrough/
    COMMAND trimja
    --expected expected.ninja
    --restrict changed.txt
)
set_tests_properties(
    trimja.snapshot.no-f
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.passthrough.fixture
)

# Check we can write to the same file we read
add_test(
    NAME trimja.big-in-out
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/bigpassthrough/
    COMMAND trimja --write --restrict changed.txt
)
set_tests_properties(
    trimja.big-in-out
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.bigpassthrough.fixture
)

# Check we can write to a different file
add_test(
    NAME trimja.--write
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/passthrough/
    COMMAND trimja --output ${CMAKE_CURRENT_BINARY_DIR}/trimja.ninja --restrict changed.txt
)
set_tests_properties(
    trimja.--write
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.passthrough.fixture
)
add_test(
    NAME trimja.--write.cmp
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${CMAKE_CURRENT_SOURCE_DIR}/tests/passthrough/expected.ninja ${CMAKE_CURRENT_BINARY_DIR}/trimja.ninja
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
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tests/simple/ $ENV{ComSpec} /c $<TARGET_FILE:trimja> < changed.txt
    )
else()
    add_test(
        NAME trimja.stdin
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tests/simple/ sh -c "$<TARGET_FILE:trimja> < changed.txt"
    )
endif()
set_property(TEST trimja.stdin PROPERTY PASS_REGULAR_EXPRESSION "build out1")
set_tests_properties(
    trimja.stdin
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.simple.fixture
)

# Snapshot tests
foreach(TEST ${TRIMJA_TESTS})
    add_test(
        NAME trimja.snapshot.${TEST}
        COMMAND trimja -f build.ninja --expected expected.ninja --restrict changed.txt
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST}/
    )
    set_tests_properties(
        trimja.snapshot.${TEST}
        PROPERTIES FIXTURES_REQUIRED trimja.snapshot.${TEST}.fixture
    )
endforeach()

