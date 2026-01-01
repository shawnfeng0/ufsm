cmake_minimum_required(VERSION 3.14)

# Extract version from header
file(STRINGS "include/ufsm/ufsm.h" UFSM_VER_LINE REGEX "UFSM_VERSION \"[0-9.]+\"")
string(REGEX MATCH "[0-9.]+" UFSM_VER "${UFSM_VER_LINE}")

project(ufsm VERSION ${UFSM_VER} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

option(UFSM_BUILD_EXAMPLES "Build examples" OFF)
option(UFSM_BUILD_TESTS "Build unit tests" OFF)

# Enable CTest integration at the top level so `ctest` can discover tests
# registered in subdirectories.
enable_testing()

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

if(UFSM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(UFSM_BUILD_TESTS)
    add_subdirectory(test)
endif()
