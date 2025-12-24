# ufsm - Ultra-lightweight Finite State Machine

[ufsm](https://github.com/shawnfeng0/ufsm) is a modern, header-only C++17 hierarchical finite state machine (HFSM) library. It is inspired by `boost::statechart` but designed to be lightweight, RTTI-free, and easy to integrate.

## Key Features

*   **Header-only**: No build steps required, just include `<ufsm/ufsm.h>`.
*   **Hierarchical**: Supports nested states (composite states) with automatic drill-down and drill-up behavior.
*   **Type-Safe**: Heavily relies on C++ templates and CRTP for compile-time checks.
*   **Efficient Memory Model**: States are managed via `std::unique_ptr` with no shared pointer overhead.
*   **Event Deferral**: Built-in support for deferring events to be processed later.
*   **Flexible Transitions**: Supports sibling transitions, hierarchical transitions, and custom actions/guards.

## Integration

Since `ufsm` is header-only, you only need to add the `include` directory to your project's include path.

```cmake
target_include_directories(my_target PRIVATE path/to/ufsm/include)
```

## Quick Start

Here is a simple example of a robot controller state machine.

```cpp
#include <iostream>
#include <ufsm/ufsm.h>

// 1. Define Events
struct EvStart : ufsm::Event<EvStart> {};
struct EvStop : ufsm::Event<EvStop> {};

// 2. Forward Declare States
struct Robot; // The Machine
struct Idle;  // Initial State
struct Running;

// 3. Define State Machine
struct Robot : ufsm::StateMachine<Robot, Idle> {
    // Shared context data
    int battery = 100;

    void OnUnhandledEvent(const ufsm::detail::EventBase& e) {
        std::cout << "Unhandled event: " << e.Name() << "\n";
    }

    // Optional: Trace all event processing results
    void OnEventProcessed(const ufsm::detail::StateBase* leaf, const ufsm::detail::EventBase& e, ufsm::Result r) {
        std::cout << "Event " << e.Name() << " processed by "
                  << (leaf ? leaf->Name() : "null") << " result: " << (int)r << "\n";
    }
};

// 4. Define States
struct Idle : ufsm::State<Idle, Robot> {
    Idle() { std::cout << "Entering Idle\n"; }

    using reactions = ufsm::List<
        ufsm::Transition<EvStart, Running>
    >;
};

struct Running : ufsm::State<Running, Robot> {
    Running() { std::cout << "Entering Running\n"; }

    // Custom reaction with guard logic
    ufsm::Result React(const EvStop&) {
        std::cout << "Stopping...\n";
        return Transit<Idle>();
    }

    using reactions = ufsm::List<
        ufsm::Reaction<EvStop>
    >;
};

int main() {
    Robot robot;
    robot.Initiate();           // Enter Idle
    robot.ProcessEvent(EvStart{}); // Idle -> Running
    robot.ProcessEvent(EvStop{});  // Running -> Idle
    return 0;
}
```

## Advanced Usage

### Hierarchical States (Composite States)

States can have children. When entering a parent state, the initial child state is automatically entered (Drill-down).

```cpp
// Active has 'Moving' as its initial substate
struct Moving;
struct Active : ufsm::State<Active, Robot, Moving> { ... };

// Moving is a child of Active
struct Moving : ufsm::State<Moving, Active> { ... };
```

### Transitions

*   **Sibling**: `Transit<SiblingState>()`
*   **Drill-down**: `Transit<ChildState>()` (Parent remains active)
*   **Drill-up**: `Transit<ParentState>()` (Exits current state and re-enters Parent)
*   **Cross-hierarchy**: `Transit<OtherBranchState>()` (Exits up to LCA, then enters down)

### Actions and Guards

You can attach actions to transitions or use `React` for complex logic.

```cpp
// Transition with Action
struct MyAction {
    void operator()() { std::cout << "Transitioning!\n"; }
};
using reactions = ufsm::List<
    ufsm::Transition<EvGo, DestState, MyAction>
>;

// Manual React with Guard
ufsm::Result React(const EvCheck& e) {
    if (Context<Robot>().battery > 10) {
        return Transit<DestState>();
    }
    return DiscardEvent();
}
```

### Event Deferral

Events can be deferred to be processed later (e.g., after a state change).

```cpp
using reactions = ufsm::List<
    ufsm::Deferral<EvBusy> // Defer EvBusy while in this state
>;
```

### Debugging & Tracing

You can add an `OnEventProcessed` method to your StateMachine class to trace every event processed by the system. This is a zero-cost abstraction (SFINAE) if not defined.

```cpp
void OnEventProcessed(const ufsm::detail::StateBase* leaf, const ufsm::detail::EventBase& e, ufsm::Result r) {
    // leaf: The state that handled (or tried to handle) the event
    // e: The event being processed
    // r: The result (Consumed, Forwarded, Deferred, etc.)
    std::cout << "[Trace] " << e.Name() << " -> " << (int)r << "\n";
}
```

## Ownership Model

`ufsm` uses a strict ownership model to ensure deterministic destruction:

*   The **StateMachine** owns the active state path via `std::unique_ptr`.
*   **States** hold a raw pointer to their context (parent/machine).
*   **Events** are polymorphic and are cloned (stored by value) when posted to the queue.

## Examples

Check the `examples/` directory for more comprehensive usage:
*   `examples/comprehensive_demo`: A full robot controller demo showcasing all features.
*   `examples/connector`: A connection manager example.

## License

See [LICENSE](LICENSE) file.
