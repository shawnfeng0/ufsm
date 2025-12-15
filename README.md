# ufsm
Ported from boost::statechart.

The basic hierarchical state transition function has been stably available.
Please refer to the examples in [examples](examples) and boost::statechart for usage. The documentation is being improved...

## Quick Start

```cpp
#include <ufsm/ufsm.hpp>

// 1) Define events
FSM_EVENT(EvPing) {};
FSM_EVENT(EvToB) {};

// Forward declarations (macros expand to `struct ...`)
struct Root;
struct StateA;
struct StateB;

// 2) Define the state machine and states
FSM_STATE_MACHINE(Root, StateA) {
	int pings = 0;
};

FSM_STATE(StateA, Root) {
	using reactions = ufsm::List<
		ufsm::Reaction<EvPing>,
		ufsm::Reaction<EvToB>
	>;

	ufsm::Result React(const EvPing&) {
		OutermostContext().pings++;
		return consume_event();
	}

	ufsm::Result React(const EvToB&) {
		return Transit<StateB>();
	}
};

FSM_STATE(StateB, Root) {
	// No reactions => will forward to parent (Root), which forwards by default.
};

int main() {
	Root machine;
	machine.Initiate();
	machine.ProcessEvent(EvPing{});
	machine.ProcessEvent(EvToB{});
	machine.Terminate();
	return 0;
}
```

## Current ownership model (no shared_ptr)

`ufsm` is a header-only hierarchical state machine.

The state machine centrally owns the active state chain (outermost → innermost) using `std::unique_ptr`.
States store a raw pointer to their parent context; parents do not own children.

Practical implications:

- You can take a non-owning pointer view via `&Context<T>()`.
- The pointer is valid only while the corresponding state is active.
- Do not cache the pointer across transitions or after `Terminate()`.

This design avoids reference-counting overhead and makes state lifetime explicit and deterministic.

## Limitations

- Events are identified by a per-binary internal type tag (RTTI-free). Cross-DSO/plugin event dispatch is not supported.
