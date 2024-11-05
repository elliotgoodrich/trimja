cmake_minimum_required(VERSION 3.26.3)
project(trimja VERSION 0.5.4)
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
    src/allocationprofiler.cpp
    src/basicscope.cpp
    src/builddirutil.cpp
    src/depsreader.cpp
    src/edgescope.cpp
    src/evalstring.cpp
    src/graph.cpp
    src/fixed_string.cpp
    src/logreader.cpp
    src/manifestparser.cpp
    src/murmur_hash.cpp
    src/ninja_clock.cpp
    src/rule.cpp
    src/trimutil.cpp
    thirdparty/ninja/lexer.cc
    thirdparty/ninja/util.cc
    $<$<BOOL:${WIN32}>:thirdparty/ninja/getopt.c>
)

target_compile_definitions(trimja PRIVATE TRIMJA_VERSION="${CMAKE_PROJECT_VERSION}")
target_include_directories(trimja PRIVATE src)
target_include_directories(trimja SYSTEM PRIVATE thirdparty)
target_compile_options(trimja PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_compile_definitions(trimja PRIVATE
    # debug iterators cause default ctor of std::string and std::vector to allocate
    $<$<CXX_COMPILER_ID:MSVC>:_ITERATOR_DEBUG_LEVEL=0>
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
)
set_property(TARGET trimja PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

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

# Generate test files for the absolute/relative unit test
get_filename_component(ABSOLUTE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tests/absolute/relative" ABSOLUTE)
string(REPLACE ":" "$:" ABSOLUTE_PATH_ESCAPED "${ABSOLUTE_PATH}")
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/tests/absolute/changed.txt" "${ABSOLUTE_PATH}\nabsolute")
string(APPEND NINJA_CONTENTS "rule copy\n")
string(APPEND NINJA_CONTENTS "  command = ninja --version \$in -> \$out\n")
string(APPEND NINJA_CONTENTS "build out_relative: copy absolute\n")
string(APPEND NINJA_CONTENTS "build out_absolute: copy ${ABSOLUTE_PATH_ESCAPED}\n")
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/tests/absolute/build.ninja" "${NINJA_CONTENTS}")
file(COPY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/tests/absolute/build.ninja" "${CMAKE_CURRENT_SOURCE_DIR}/tests/absolute/expected.ninja")

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
        COMMAND ninja -d keepdepfile -d keeprsp -f bootstrap.ninja
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
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/passthrough/
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
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/bigpassthrough/
    COMMAND trimja --write --affected changed.txt
)
set_tests_properties(
    trimja.big-in-out
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.bigpassthrough.fixture
)

# Check we can write to a different file
add_test(
    NAME trimja.--write
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/passthrough/
    COMMAND trimja --output ${CMAKE_CURRENT_BINARY_DIR}/trimja.ninja --affected changed.txt
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
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tests/simple/ $ENV{ComSpec} /c $<TARGET_FILE:trimja> - < changed.txt
    )
else()
    add_test(
        NAME trimja.stdin
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tests/simple/ sh -c "$<TARGET_FILE:trimja> - < changed.txt"
    )
endif()
set_property(TEST trimja.stdin PROPERTY PASS_REGULAR_EXPRESSION "build out1")
set_tests_properties(
    trimja.stdin
    PROPERTIES FIXTURES_REQUIRED trimja.snapshot.simple.fixture
)

# --memory
if(WIN32)
    add_test(
        NAME trimja.snapshot.--memory
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/passthrough/
        COMMAND trimja
        --memory=3
        --expected expected.ninja
        --affected changed.txt
    )
    set_tests_properties(
        trimja.snapshot.--memory
        PROPERTIES FIXTURES_REQUIRED trimja.snapshot.passthrough.fixture
    )
endif()

# --builddir
add_test(
    NAME trimja.--builddir
    COMMAND trimja -f builddir/build.ninja --builddir
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
set_property(TEST trimja.--builddir PROPERTY PASS_REGULAR_EXPRESSION "builddir\/x64\/build")

# Snapshot tests
foreach(TEST ${TRIMJA_TESTS})
    add_test(
        NAME trimja.snapshot.${TEST}
        COMMAND trimja -f ${TEST}/build.ninja --expected ${TEST}/expected.ninja --affected ${TEST}/changed.txt --explain
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    set_tests_properties(
        trimja.snapshot.${TEST}
        PROPERTIES FIXTURES_REQUIRED trimja.snapshot.${TEST}.fixture
    )

    # Check that --builddir at least returns success on all tests
    add_test(
        NAME trimja.smoke.builddir.${TEST}
        COMMAND trimja -f ${TEST}/build.ninja --builddir
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
endforeach()

