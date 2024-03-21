cmake_minimum_required(VERSION 3.4...3.18)
project(embedding_search)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ version selection")

add_subdirectory(pybind11)
pybind11_add_module(embedding_search src/main.cpp)

# EXAMPLE_VERSION_INFO is defined by setup.py and passed into the C++ code as a
# define (VERSION_INFO) here.
target_compile_definitions(embedding_search
                           PRIVATE VERSION_INFO=${EXAMPLE_VERSION_INFO})
