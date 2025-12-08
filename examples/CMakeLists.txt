add_executable(example-connector connector/connector.cc)
target_link_libraries(example-connector PRIVATE ufsm)

add_executable(example-multi_layer_state multi_layer_state_transition/main.cc)
target_link_libraries(example-multi_layer_state PRIVATE ufsm)

add_executable(example-auto_react_c17 auto_react_c17/main.cc)
target_link_libraries(example-auto_react_c17 PRIVATE ufsm)
target_compile_features(example-auto_react_c17 PRIVATE cxx_std_17)
