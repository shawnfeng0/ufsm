cmake_minimum_required(VERSION 3.17)
project(ufsm)

set(CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_executable(example-connector examples/connector.cc)
