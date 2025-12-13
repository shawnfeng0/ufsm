# ufsm Unit Tests

This project uses GoogleTest/GoogleMock for unit testing.

## Build

From the repository root:

```bash
cmake -S . -B build_test -DUFSM_BUILD_TESTS=ON
cmake --build build_test -j$(nproc)
```

## Run

### Option 1: Run the test executable directly

```bash
# Run all tests
./build_test/test/ufsm_test

# Run a specific test suite
./build_test/test/ufsm_test --gtest_filter="MultiLayerLifecycleTest.*"

# Run a specific test case
./build_test/test/ufsm_test --gtest_filter="MultiLayerLifecycleTest.ExampleExecutionFlow"
```

### Option 2: Run via CTest

```bash
ctest --test-dir build_test --output-on-failure
```

## Current Coverage

All test cases are built into a single binary: `build_test/test/ufsm_test`.

### Test files

- `test/test_multi_layer_lifecycle.cpp`: multi-layer construction/destruction order (example-driven), plus deep termination coverage
- `test/test_transition_boundary_behavior.cpp`: sibling vs top-level transition boundary semantics (minimal repro cases)
- `test/test_event_routing_behavior.cpp`: `forward_event()` / `discard_event()` / unhandled routing behavior
- `test/test_idempotency_behavior.cpp`: repeated `Initiate()` / `Terminate()` and post-terminate event behavior
- `test/test_destructor_behavior.cpp`: `~StateMachine()` must fully unwind active hierarchy
- `test/test_context_access_behavior.cpp`: `Context<T>()`/`ContextPtr<T>()` (raw pointer)/`OutermostContext()` correctness across transitions

## State Hierarchy Used in Tests

The tests use the following hierarchy:

```
MultiLayerMachine
├── StateA (initial state)
│   ├── StateAA
│   │   └── StateAAA
│   │       └── StateAAAA (leaf state, handles events)
│   └── StateAB
│       └── StateABA
│           └── StateABAA
│               └── StateABAAA (leaf state)
├── StateB
│   └── StateBA (leaf state)
└── StateC (leaf state)
```

## Key Assertions

- Construction order: parent-to-child.
- Destruction order: child-to-parent (reverse of construction).
- Termination must fully unwind the active state hierarchy.

## Adding New Tests

Add a new `test/test_*.cpp` file and list it in `test/CMakeLists.txt` under the `ufsm_test` target.
