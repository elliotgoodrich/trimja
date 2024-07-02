cmake_minimum_required(VERSION 3.26.3)
project(trimja VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

###################
add_library(
    common
    STATIC
    src/lexer.h src/lexer.cc
)
