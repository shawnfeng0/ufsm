# Google Test configuration
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Build a single test binary from all test sources in this directory.
add_executable(ufsm_test
  test_declarative_transition_behavior.cc
  test_drill_down_behavior.cc
  test_deferral_behavior.cc
  test_deferred_event_queue_behavior.cc
  test_event_name_behavior.cc
  test_multi_layer_lifecycle.cc
  test_destructor_behavior.cc
  test_event_routing_behavior.cc
  test_idempotency_behavior.cc
  test_state_query_behavior.cc
  test_transition_boundary_behavior.cc
  test_context_access_behavior.cc
  test_unhandled_event_hook_behavior.cc
  test_entry_exit.cc
)

target_link_libraries(ufsm_test
  PRIVATE
    ${PROJECT_NAME}
    gtest_main
    gmock_main
)

target_compile_options(ufsm_test PRIVATE -Wall -Wextra)

target_include_directories(ufsm_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

include(GoogleTest)
gtest_discover_tests(ufsm_test)
