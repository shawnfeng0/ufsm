
add_executable(example-connector connector/connector.cc)
target_link_libraries(example-connector PRIVATE ufsm)
target_compile_options(example-connector PRIVATE -Wall -Wextra)

add_executable(example-multi_layer_state multi_layer_state_transition/main.cc)
target_link_libraries(example-multi_layer_state PRIVATE ufsm)
target_compile_options(example-multi_layer_state PRIVATE -Wall -Wextra)

add_executable(example-comprehensive comprehensive_demo/main.cpp)
target_link_libraries(example-comprehensive PRIVATE ufsm)
target_compile_options(example-comprehensive PRIVATE -Wall -Wextra)
