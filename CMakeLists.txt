cmake_minimum_required(VERSION 3.28)

project(apex653
    VERSION 0.1.0
    DESCRIPTION "Modern C++ implementation of the ARINC 653 APEX interface"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(warnings)

option(APEX653_BUILD_EXAMPLES "Build apex653 examples" OFF)

add_subdirectory(components/apex653)

if(APEX653_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
