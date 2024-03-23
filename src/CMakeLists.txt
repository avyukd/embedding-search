set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

cmake_minimum_required(VERSION 3.4...3.18)
project(embedding_search_cpp)

add_executable(test_integration test_integration.cpp)
add_executable(test_inverted_index test_inverted_index.cpp)
