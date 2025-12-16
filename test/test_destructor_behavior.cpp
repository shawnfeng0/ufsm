#include <gtest/gtest.h>

#include <ufsm/ufsm.hpp>
#include <vector>

#include "test_support.h"

FSM_EVENT(DEventGoToB){};

struct DestructorBehaviorTag;
using DestructorBehaviorLog = LifecycleLog<DestructorBehaviorTag>;

#define TRACK_LIFECYCLE(TYPE)                                  \
 public:                                                       \
  TYPE() { DestructorBehaviorLog::RecordConstruction(#TYPE); } \
  ~TYPE() { DestructorBehaviorLog::RecordDestruction(#TYPE); }

struct DStateB;

FSM_STATE_MACHINE(DestructorMachine, DStateA) { int counter = 0; };

// DStateA -> DStateAA -> DStateAAA -> DStateAAAA
FSM_STATE(DStateA, DestructorMachine, DStateAA) { TRACK_LIFECYCLE(DStateA); };

FSM_STATE(DStateAA, DStateA, DStateAAA) { TRACK_LIFECYCLE(DStateAA); };

FSM_STATE(DStateAAA, DStateAA, DStateAAAA) { TRACK_LIFECYCLE(DStateAAA); };

FSM_STATE(DStateAAAA, DStateAAA) {
  using reactions = ufsm::List<ufsm::Reaction<DEventGoToB> >;

  ufsm::Result React(const DEventGoToB&) {
    OutermostContext().counter++;
    return Transit<DStateB>();
  }

  TRACK_LIFECYCLE(DStateAAAA);
};

// DStateB -> DStateBA
FSM_STATE(DStateB, DestructorMachine, DStateBA) { TRACK_LIFECYCLE(DStateB); };

FSM_STATE(DStateBA, DStateB) { TRACK_LIFECYCLE(DStateBA); };

TEST(DestructorBehaviorTest, DestructorTerminatesInAllScenarios) {
  // Scenario 1: destructor terminates after Initiate.
  DestructorBehaviorLog::Clear();
  {
    DestructorMachine machine;
    machine.Initiate();

    // Ensure the active hierarchy was constructed.
    ExpectSeq(DestructorBehaviorLog::Construction(), {
                                                         "DStateA",
                                                         "DStateAA",
                                                         "DStateAAA",
                                                         "DStateAAAA",
                                                     });

    // Only validate destructor behavior in the next scope exit.
    DestructorBehaviorLog::Clear();
  }

  // Leaving scope must fully unwind the active hierarchy.
  ExpectSeq(DestructorBehaviorLog::Destruction(), {
                                                      "DStateAAAA",
                                                      "DStateAAA",
                                                      "DStateAA",
                                                      "DStateA",
                                                  });

  // Scenario 2: destructor terminates after a cross top-level transition.
  DestructorBehaviorLog::Clear();
  {
    DestructorMachine machine;
    machine.Initiate();
    machine.ProcessEvent(DEventGoToB{});

    // Only validate destructor behavior in the next scope exit.
    DestructorBehaviorLog::Clear();
  }

  // After the transition, the active hierarchy should be DStateB -> DStateBA.
  ExpectSeq(DestructorBehaviorLog::Destruction(), {
                                                      "DStateBA",
                                                      "DStateB",
                                                  });
}
