cmake_minimum_required(VERSION 3.0.0)
project(mytest VERSION 0.1.0 LANGUAGES C CXX)

include(${PROJECT_SOURCE_DIR}/hammer/cmake/cpp_standard.cmake)

add_executable(mytest test.cpp)
target_include_directories(mytest PUBLIC etherfabric/)
target_link_directories(mytest PUBLIC .)
target_link_libraries(mytest PUBLIC
    hammer::header    
    hammer::ipc
    ciul1
    pthread)

add_library(efvi STATIC EfviWrapper.cpp)
target_include_directories(efvi PUBLIC .)
target_link_libraries(efvi PUBLIC
    hammer::header
    hammer::fmt
    ciul1)

add_subdirectory(${PROJECT_SOURCE_DIR}/hammer hammer)