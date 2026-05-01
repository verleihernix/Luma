cmake_minimum_required(VERSION 3.20)
project(luma VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Library ──
add_library(luma_core STATIC
    src/lexer.cpp
    src/parser.cpp
    src/interpreter.cpp
    src/vm.cpp
)
target_include_directories(luma_core
    PUBLIC  include
    PRIVATE src
)
target_compile_options(luma_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ── Example host ──
add_executable(luma examples/main.cpp)
target_link_libraries(luma PRIVATE luma_core)