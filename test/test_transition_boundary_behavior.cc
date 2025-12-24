#include <gtest/gtest.h>

#include <ufsm/ufsm.h>
#include <vector>

#include "test_support.h"

FSM_EVENT(TBEventToB){};
FSM_EVENT(TBEventToBWithAction){};
FSM_EVENT(TBEventToBWithNoArgAction){};
FSM_EVENT(TBEventToTop2){};
FSM_EVENT(TBEventToTop2WithAction){};

struct TransitionBoundaryBehaviorTag;
using TransitionBoundaryBehaviorLog = LifecycleLog<TransitionBoundaryBehaviorTag>;

#define TRACK_LIFECYCLE(TYPE)                                          \
 public:                                                               \
  TYPE() { TransitionBoundaryBehaviorLog::RecordConstruction(#TYPE); } \
  ~TYPE() { TransitionBoundaryBehaviorLog::RecordDestruction(#TYPE); }

// Case 1: Sibling transition under a common parent.
// Expectation: the common parent remains alive and must not be destroyed.
struct TBParent;
struct TBLeafA;
struct TBLeafB;

FSM_STATE_MACHINE(TransitionBoundaryMachine, TBParent) {
  int counter = 0;
  int action_counter = 0;
};

FSM_STATE(TBParent, TransitionBoundaryMachine, TBLeafA) { TRACK_LIFECYCLE(TBParent); };

FSM_STATE(TBLeafA, TBParent) {
  using reactions = ufsm::List<ufsm::Reaction<TBEventToB>, ufsm::Reaction<TBEventToBWithAction>,
                               ufsm::Reaction<TBEventToBWithNoArgAction> >;

  ufsm::Result React(const TBEventToB&) {
    OutermostContext().counter++;
    return Transit<TBLeafB>();
  }

  ufsm::Result React(const TBEventToBWithAction&) {
    return Transit<TBLeafB>([](TBParent& parent) { parent.OutermostContext().action_counter++; });
  }

  ufsm::Result React(const TBEventToBWithNoArgAction&) {
    int* p = &OutermostContext().action_counter;
    return Transit<TBLeafB>([p]() { (*p)++; });
  }

  TRACK_LIFECYCLE(TBLeafA);
};

FSM_STATE(TBLeafB, TBParent) { TRACK_LIFECYCLE(TBLeafB); };

// Case 2: Cross top-level transition.
// Expectation: the entire previous top-level subtree is destroyed.
struct TBTop1;
struct TBTop1Leaf;
struct TBTop2;

FSM_STATE_MACHINE(TransitionTopLevelMachine, TBTop1) {
  int counter = 0;
  int action_counter = 0;
};

FSM_STATE(TBTop1, TransitionTopLevelMachine, TBTop1Leaf) { TRACK_LIFECYCLE(TBTop1); };

FSM_STATE(TBTop1Leaf, TBTop1) {
  using reactions = ufsm::List<ufsm::Reaction<TBEventToTop2>, ufsm::Reaction<TBEventToTop2WithAction> >;

  ufsm::Result React(const TBEventToTop2&) {
    OutermostContext().counter++;
    return Transit<TBTop2>();
  }

  ufsm::Result React(const TBEventToTop2WithAction&) {
    return Transit<TBTop2>([](TransitionTopLevelMachine& machine) { machine.action_counter++; });
  }

  TRACK_LIFECYCLE(TBTop1Leaf);
};

FSM_STATE(TBTop2, TransitionTopLevelMachine) { TRACK_LIFECYCLE(TBTop2); };

TEST(TransitionBoundaryBehaviorTest, BoundarySemanticsInAllScenarios) {
  // Scenario 1: sibling transition under a common parent.
  {
    TransitionBoundaryMachine machine;
    machine.Initiate();

    // Ignore initial construction noise.
    TransitionBoundaryBehaviorLog::Clear();
    const auto res = machine.ProcessEvent(TBEventToB{});
    EXPECT_EQ(res, ufsm::Result::kConsumed);

    // Only the leaf should be replaced.
    ExpectSeq(TransitionBoundaryBehaviorLog::Destruction(), {"TBLeafA"});
    ExpectSeq(TransitionBoundaryBehaviorLog::Construction(), {"TBLeafB"});
    EXPECT_EQ(machine.counter, 1);
  }

  // Scenario 2: cross top-level transition destroys the old top-level subtree.
  {
    TransitionTopLevelMachine machine;
    machine.Initiate();

    TransitionBoundaryBehaviorLog::Clear();
    const auto res = machine.ProcessEvent(TBEventToTop2{});
    EXPECT_EQ(res, ufsm::Result::kConsumed);

    ExpectSeq(TransitionBoundaryBehaviorLog::Destruction(), {"TBTop1Leaf", "TBTop1"});
    ExpectSeq(TransitionBoundaryBehaviorLog::Construction(), {"TBTop2"});
    EXPECT_EQ(machine.counter, 1);
  }
}

TEST(TransitionActionTest, ExecutesOnLeastCommonAncestorContext) {
  // Scenario 1: sibling transition under a common parent executes action on TBParent.
  {
    TransitionBoundaryMachine machine;
    machine.Initiate();

    TransitionBoundaryBehaviorLog::Clear();
    const auto res = machine.ProcessEvent(TBEventToBWithAction{});
    EXPECT_EQ(res, ufsm::Result::kConsumed);

    ExpectSeq(TransitionBoundaryBehaviorLog::Destruction(), {"TBLeafA"});
    ExpectSeq(TransitionBoundaryBehaviorLog::Construction(), {"TBLeafB"});
    EXPECT_EQ(machine.action_counter, 1);
    EXPECT_EQ(machine.counter, 0);
  }

  // Scenario 1b: sibling transition supports no-arg action().
  {
    TransitionBoundaryMachine machine;
    machine.Initiate();

    TransitionBoundaryBehaviorLog::Clear();
    const auto res = machine.ProcessEvent(TBEventToBWithNoArgAction{});
    EXPECT_EQ(res, ufsm::Result::kConsumed);

    ExpectSeq(TransitionBoundaryBehaviorLog::Destruction(), {"TBLeafA"});
    ExpectSeq(TransitionBoundaryBehaviorLog::Construction(), {"TBLeafB"});
    EXPECT_EQ(machine.action_counter, 1);
    EXPECT_EQ(machine.counter, 0);
  }

  // Scenario 2: cross top-level transition executes action on the state machine (common context).
  {
    TransitionTopLevelMachine machine;
    machine.Initiate();

    TransitionBoundaryBehaviorLog::Clear();
    const auto res = machine.ProcessEvent(TBEventToTop2WithAction{});
    EXPECT_EQ(res, ufsm::Result::kConsumed);

    ExpectSeq(TransitionBoundaryBehaviorLog::Destruction(), {"TBTop1Leaf", "TBTop1"});
    ExpectSeq(TransitionBoundaryBehaviorLog::Construction(), {"TBTop2"});
    EXPECT_EQ(machine.action_counter, 1);
    EXPECT_EQ(machine.counter, 0);
  }
}
