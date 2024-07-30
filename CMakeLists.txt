cmake_minimum_required(VERSION 3.26.3)
project(trimja VERSION 0.0.1)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    trimja
    src/trimja.m.cpp
    src/lexer.cc
    src/trimutil.cc
    $<$<BOOL:${WIN32}>:src/getopt.c>
)
target_compile_definitions(trimja PRIVATE TRIMJA_VERSION="${CMAKE_PROJECT_VERSION}")

# Breathing test
add_test(
    NAME breathing
    COMMAND trimja
    -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/passthrough.ninja
    --changed ${CMAKE_CURRENT_SOURCE_DIR}/tests/passthrough.changed.txt
)

# --version
add_test(NAME --version COMMAND trimja --version)
string(REPLACE "." "\." TRIMJA_VERSION_REGEX ${CMAKE_PROJECT_VERSION})
set_property(
    TEST --version
    PROPERTY PASS_REGULAR_EXPRESSION "^${TRIMJA_VERSION_REGEX}\n$"
)

# --help
add_test(NAME --help COMMAND trimja --help)
set_property(TEST --help PROPERTY PASS_REGULAR_EXPRESSION "Help")
add_test(NAME -h COMMAND trimja -h)
set_property(TEST -h PROPERTY PASS_REGULAR_EXPRESSION "Help")

# --unknown
add_test(NAME --unknown COMMAND trimja --unknown)
set_property(TEST --unknown PROPERTY WILL_FAIL true)
add_test(NAME -u COMMAND trimja -u)
set_property(TEST -u PROPERTY WILL_FAIL true)

# Snapshot tests
foreach(TEST passthrough simple simple_reversed chained fan pyramid dependencies attached)
    add_test(
        NAME ${TEST}
        COMMAND trimja
        -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST}.ninja
        --expected ${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST}.expected.ninja
        --changed ${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST}.changed.txt
    )
endforeach()
