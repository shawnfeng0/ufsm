cmake_minimum_required(VERSION 3.17)
project(ufsm)

set(CMAKE_CXX_STANDARD 11)

option(UFSM_BUILD_EXAMPLES "Build examples" OFF)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

if (UFSM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
