#include <gtest/gtest.h>

#include <string>
#include <ufsm/ufsm.h>
#include <vector>

#include "test_support.h"

// Event types for multi-layer state machine
FSM_EVENT(EventGoToABAA){};
FSM_EVENT(EventGoBackToAAAA){};
FSM_EVENT(EventGoToB){};
FSM_EVENT(EventGoToC){};

struct MultiLayerLifecycleTag;
using MultiLayerLifecycleLog = LifecycleLog<MultiLayerLifecycleTag>;

// Macro for tracking lifecycle in test states
#define TRACK_LIFECYCLE(TYPE)                                   \
 public:                                                        \
  TYPE() { MultiLayerLifecycleLog::RecordConstruction(#TYPE); } \
  ~TYPE() { MultiLayerLifecycleLog::RecordDestruction(#TYPE); }

// Multi-layer state machine definition
FSM_STATE_MACHINE(MultiLayerMachine, StateA) { int counter = 0; };

// State hierarchy:
// StateA (with StateAA as initial substate)
//   -> StateAA (with StateAAA as initial substate)
//      -> StateAAA (with StateAAAA as initial substate)
//         -> StateAAAA (leaf state, can handle events)
//   -> StateAB (with StateABA as initial substate)
//      -> StateABA (with StateABAA as initial substate)
//         -> StateABAA (with StateABAAA as initial substate)
//            -> StateABAAA (leaf state)
// StateB (with StateBA as initial substate)
//   -> StateBA (leaf state)
// StateC (leaf state, no substates)

// Forward declarations for all states
struct StateABAA;
struct StateB;
struct StateC;

// StateA hierarchy
FSM_STATE(StateA, MultiLayerMachine, StateAA) {
  std::string context_data;
  TRACK_LIFECYCLE(StateA);
};

FSM_STATE(StateAA, StateA, StateAAA) { TRACK_LIFECYCLE(StateAA); };

FSM_STATE(StateAAA, StateAA, StateAAAA) { TRACK_LIFECYCLE(StateAAA); };

FSM_STATE(StateAAAA, StateAAA) {
  using reactions = ufsm::List<ufsm::Reaction<EventGoToABAA>, ufsm::Reaction<EventGoToB> >;

  ufsm::Result React(const EventGoToABAA&) {
    Context<StateA>().context_data = "transitioned_to_ABAA";
    OutermostContext().counter++;
    return Transit<StateABAA>();
  }

  ufsm::Result React(const EventGoToB&) {
    OutermostContext().counter++;
    return Transit<StateB>();
  }

  TRACK_LIFECYCLE(StateAAAA);
};

// StateAB hierarchy
FSM_STATE(StateAB, StateA, StateABA) { TRACK_LIFECYCLE(StateAB); };

FSM_STATE(StateABA, StateAB, StateABAA) {
  using reactions = ufsm::List<ufsm::Reaction<EventGoBackToAAAA> >;

  ufsm::Result React(const EventGoBackToAAAA&) {
    OutermostContext().counter++;
    return Transit<StateAAAA>();
  }

  TRACK_LIFECYCLE(StateABA);
};

FSM_STATE(StateABAA, StateABA, StateABAAA) { TRACK_LIFECYCLE(StateABAA); };

FSM_STATE(StateABAAA, StateABAA) { TRACK_LIFECYCLE(StateABAAA); };

// StateB hierarchy
FSM_STATE(StateB, MultiLayerMachine, StateBA) {
  using reactions = ufsm::List<ufsm::Reaction<EventGoToC> >;

  ufsm::Result React(const EventGoToC&) {
    OutermostContext().counter++;
    return Transit<StateC>();
  }

  TRACK_LIFECYCLE(StateB);
};

FSM_STATE(StateBA, StateB) { TRACK_LIFECYCLE(StateBA); };

// StateC (simple state, no substates)
FSM_STATE(StateC, MultiLayerMachine) { TRACK_LIFECYCLE(StateC); };

// Test fixture
class MultiLayerLifecycleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MultiLayerLifecycleLog::Clear();
    machine = std::make_unique<MultiLayerMachine>();
  }

  void TearDown() override {
    machine->Terminate();
    machine.reset();
  }

  std::unique_ptr<MultiLayerMachine> machine;
};

// Follow the multi_layer_lifecycle example flow: after each event, verify the per-event
// construction/destruction sequence produced by that event.
TEST_F(MultiLayerLifecycleTest, ExampleExecutionFlow) {
  // Step 0: Initiate
  MultiLayerLifecycleLog::Clear();
  machine->Initiate();
  ExpectSeq(MultiLayerLifecycleLog::Destruction(), {});
  ExpectSeq(MultiLayerLifecycleLog::Construction(), {
                                                        "StateA",
                                                        "StateAA",
                                                        "StateAAA",
                                                        "StateAAAA",
                                                    });

  // Step 1: StateAAAA -> StateABAA (under common parent StateA)
  MultiLayerLifecycleLog::Clear();
  machine->ProcessEvent(EventGoToABAA{});
  ExpectSeq(MultiLayerLifecycleLog::Destruction(), {
                                                       "StateAAAA",
                                                       "StateAAA",
                                                       "StateAA",
                                                   });
  ExpectSeq(MultiLayerLifecycleLog::Construction(), {
                                                        "StateAB",
                                                        "StateABA",
                                                        "StateABAA",
                                                        "StateABAAA",
                                                    });

  // Step 2: StateABAA -> StateAAAA (under common parent StateA)
  MultiLayerLifecycleLog::Clear();
  machine->ProcessEvent(EventGoBackToAAAA{});
  ExpectSeq(MultiLayerLifecycleLog::Destruction(), {
                                                       "StateABAAA",
                                                       "StateABAA",
                                                       "StateABA",
                                                       "StateAB",
                                                   });
  ExpectSeq(MultiLayerLifecycleLog::Construction(), {
                                                        "StateAA",
                                                        "StateAAA",
                                                        "StateAAAA",
                                                    });

  // Step 3: StateAAAA -> StateB (cross top-level)
  MultiLayerLifecycleLog::Clear();
  machine->ProcessEvent(EventGoToB{});
  ExpectSeq(MultiLayerLifecycleLog::Destruction(), {
                                                       "StateAAAA",
                                                       "StateAAA",
                                                       "StateAA",
                                                       "StateA",
                                                   });
  ExpectSeq(MultiLayerLifecycleLog::Construction(), {
                                                        "StateB",
                                                        "StateBA",
                                                    });

  // Step 4: StateB -> StateC
  MultiLayerLifecycleLog::Clear();
  machine->ProcessEvent(EventGoToC{});
  ExpectSeq(MultiLayerLifecycleLog::Destruction(), {
                                                       "StateBA",
                                                       "StateB",
                                                   });
  ExpectSeq(MultiLayerLifecycleLog::Construction(), {
                                                        "StateC",
                                                    });

  // Step 5: Terminate
  MultiLayerLifecycleLog::Clear();
  machine->Terminate();
  ExpectSeq(MultiLayerLifecycleLog::Destruction(), {
                                                       "StateC",
                                                   });
  ExpectSeq(MultiLayerLifecycleLog::Construction(), {});
}

// Combine multiple terminate scenarios into a single test to reduce test-case count,
// while keeping the same coverage and assertions.
TEST_F(MultiLayerLifecycleTest, TerminateFromVariousStates) {
  // Scenario 1: Terminate directly from the initial deep state (StateA->StateAA->StateAAA->StateAAAA).
  machine = std::make_unique<MultiLayerMachine>();
  machine->Initiate();
  MultiLayerLifecycleLog::Clear();
  machine->Terminate();
  ExpectSeq(MultiLayerLifecycleLog::Destruction(), {
                                                       "StateAAAA",
                                                       "StateAAA",
                                                       "StateAA",
                                                       "StateA",
                                                   });
  ExpectSeq(MultiLayerLifecycleLog::Construction(), {});

  // Scenario 2: Terminate from a deeper branch state (StateA->StateAB->StateABA->StateABAA->StateABAAA).
  machine = std::make_unique<MultiLayerMachine>();
  machine->Initiate();
  machine->ProcessEvent(EventGoToABAA{});
  MultiLayerLifecycleLog::Clear();
  machine->Terminate();
  ExpectSeq(MultiLayerLifecycleLog::Destruction(), {
                                                       "StateABAAA",
                                                       "StateABAA",
                                                       "StateABA",
                                                       "StateAB",
                                                       "StateA",
                                                   });
  ExpectSeq(MultiLayerLifecycleLog::Construction(), {});

  // Scenario 3: Terminate from the two-level nested state (StateB/StateBA).
  machine = std::make_unique<MultiLayerMachine>();
  machine->Initiate();
  machine->ProcessEvent(EventGoToB{});
  MultiLayerLifecycleLog::Clear();
  machine->Terminate();
  ExpectSeq(MultiLayerLifecycleLog::Destruction(), {
                                                       "StateBA",
                                                       "StateB",
                                                   });
  ExpectSeq(MultiLayerLifecycleLog::Construction(), {});
}
