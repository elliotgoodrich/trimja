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
    src/trimutil.cpp
    thirdparty/ninja/lexer.cc
    $<$<BOOL:${WIN32}>:thirdparty/ninja/getopt.c>
)
target_compile_definitions(trimja PRIVATE TRIMJA_VERSION="${CMAKE_PROJECT_VERSION}")
target_include_directories(trimja PRIVATE thirdparty)
target_compile_definitions(trimja PRIVATE JSON_DIAGNOSTICS=1)

# --version
add_test(NAME trimja.--version COMMAND trimja --version)
string(REPLACE "." "\." TRIMJA_VERSION_REGEX ${CMAKE_PROJECT_VERSION})
set_property(
    TEST trimja.--version
    PROPERTY PASS_REGULAR_EXPRESSION "^${TRIMJA_VERSION_REGEX}\n$"
)

# --help
add_test(NAME trimja.--help COMMAND trimja --help)
set_property(TEST trimja.--help PROPERTY PASS_REGULAR_EXPRESSION "Help")
add_test(NAME trimja.-h COMMAND trimja -h)
set_property(TEST trimja.-h PROPERTY PASS_REGULAR_EXPRESSION "Help")

# --unknown
add_test(NAME trimja.--unknown COMMAND trimja --unknown)
set_property(TEST trimja.--unknown PROPERTY WILL_FAIL true)
add_test(NAME trimja.-u COMMAND trimja -u)
set_property(TEST trimja.-u PROPERTY WILL_FAIL true)

# Snapshot tests
foreach(TEST passthrough simple simple_reversed chained fan pyramid dependencies attached basic_dyndep)
    add_test(
        NAME trimja.snapshot.${TEST}
        COMMAND trimja
        -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/${TEST}/build.ninja
        --expected ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/${TEST}/expected.ninja
        --changed ${CMAKE_CURRENT_SOURCE_DIR}/tests/trimja/${TEST}/changed.txt
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
    add_test(
        NAME kinja.snapshot.${TEST}
        COMMAND kinja
        --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/in.json
        --output ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/actual.dat
    )
    add_test(
        NAME kinja.snapshot.${TEST}.cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/actual.dat
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/expected.dat
    )
    set_tests_properties(
        kinja.snapshot.${TEST}.cmp
        PROPERTIES FIXTURES_REQUIRED kinja.snapshot.${TEST}.fixture
    )
    set_tests_properties(kinja.snapshot.${TEST}
        PROPERTIES FIXTURES_SETUP kinja.snapshot.${TEST}.fixture
    )

    add_test(
        NAME kinja.snapshot.prettyprint.${TEST}
        COMMAND kinja
        --print ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/basic/actual.dat
        --output ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/basic/actual.json
    )
    add_test(
        NAME kinja.snapshot.prettyprint.${TEST}.cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/in.json
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/kinja/${TEST}/actual.json
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
