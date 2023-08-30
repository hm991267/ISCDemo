cmake_minimum_required(VERSION 3.0.0)
project(mytest VERSION 0.1.0 LANGUAGES C CXX)

include(${PROJECT_SOURCE_DIR}/../hammer/cmake/cpp_standard.cmake)


add_library(ipc STATIC MediaEfvi.cpp EfviWrapper.cpp)
target_link_libraries(ipc
    PUBLIC
        hammer::header    
        hammer::ipc
        pthread
)

add_executable(mytest test.cpp)
# target_include_directories(mytest PUBLIC etherfabric/)
# target_link_directories(mytest PUBLIC .)
target_link_libraries(mytest
    PUBLIC
        ipc
)

option(USE_EFVI ON)

add_compile_options(-g -O0)

if (USE_EFVI)
target_compile_definitions(ipc
    PRIVATE
        -DUSE_EFVI
)
target_compile_definitions(mytest
    PRIVATE
        -DUSE_EFVI
)
target_link_directories(ipc
    PUBLIC
        .
)
target_link_libraries(ipc
    PUBLIC
        ciul1
)
target_include_directories(ipc
    PUBLIC
        .
)
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/../hammer hammer)