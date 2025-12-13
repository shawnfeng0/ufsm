
add_executable(example-connector connector/connector.cc)
target_link_libraries(example-connector PRIVATE ufsm)
target_compile_options(example-connector PRIVATE -Wall -Wextra)

add_executable(example-multi_layer_state multi_layer_state_transition/main.cc)
target_link_libraries(example-multi_layer_state PRIVATE ufsm)
target_compile_options(example-multi_layer_state PRIVATE -Wall -Wextra)
