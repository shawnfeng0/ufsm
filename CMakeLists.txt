cmake_minimum_required(VERSION 3.5)
project(ufsm)

set(CMAKE_CXX_STANDARD 17)

option(UFSM_BUILD_EXAMPLES "Build examples" OFF)

add_compile_options(-Wextra -Wall)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

    add_subdirectory(examples)
