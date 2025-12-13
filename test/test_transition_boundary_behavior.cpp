#include <gtest/gtest.h>
#include <ufsm/ufsm.hpp>

#include "test_support.h"

#include <vector>

FSM_EVENT(TBEventToB) {};
FSM_EVENT(TBEventToTop2) {};

struct TransitionBoundaryBehaviorTag;
using TransitionBoundaryBehaviorLog = LifecycleLog<TransitionBoundaryBehaviorTag>;

#define TRACK_LIFECYCLE(TYPE) \
 public: \
    TYPE() { TransitionBoundaryBehaviorLog::RecordConstruction(#TYPE); } \
    ~TYPE() { TransitionBoundaryBehaviorLog::RecordDestruction(#TYPE); }

// Case 1: Sibling transition under a common parent.
// Expectation: the common parent remains alive and must not be destroyed.
struct TBParent;
struct TBLeafA;
struct TBLeafB;

FSM_STATE_MACHINE(TransitionBoundaryMachine, TBParent) {
    int counter = 0;
};

FSM_STATE(TBParent, TransitionBoundaryMachine, TBLeafA) {
    TRACK_LIFECYCLE(TBParent);
};

FSM_STATE(TBLeafA, TBParent) {
    using reactions = ufsm::mp::List<
        ufsm::Reaction<TBEventToB>
    >;

    ufsm::Result React(const TBEventToB&) {
        OutermostContext().counter++;
        return Transit<TBLeafB>();
    }

    TRACK_LIFECYCLE(TBLeafA);
};

FSM_STATE(TBLeafB, TBParent) {
    TRACK_LIFECYCLE(TBLeafB);
};

// Case 2: Cross top-level transition.
// Expectation: the entire previous top-level subtree is destroyed.
struct TBTop1;
struct TBTop1Leaf;
struct TBTop2;

FSM_STATE_MACHINE(TransitionTopLevelMachine, TBTop1) {
    int counter = 0;
};

FSM_STATE(TBTop1, TransitionTopLevelMachine, TBTop1Leaf) {
    TRACK_LIFECYCLE(TBTop1);
};

FSM_STATE(TBTop1Leaf, TBTop1) {
    using reactions = ufsm::mp::List<
        ufsm::Reaction<TBEventToTop2>
    >;

    ufsm::Result React(const TBEventToTop2&) {
        OutermostContext().counter++;
        return Transit<TBTop2>();
    }

    TRACK_LIFECYCLE(TBTop1Leaf);
};

FSM_STATE(TBTop2, TransitionTopLevelMachine) {
    TRACK_LIFECYCLE(TBTop2);
};

TEST(TransitionBoundaryBehaviorTest, BoundarySemanticsInAllScenarios) {
    // Scenario 1: sibling transition under a common parent.
    {
        TransitionBoundaryMachine machine;
        machine.Initiate();

        // Ignore initial construction noise.
        TransitionBoundaryBehaviorLog::Clear();
        machine.ProcessEvent(TBEventToB{});

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
        machine.ProcessEvent(TBEventToTop2{});

        ExpectSeq(TransitionBoundaryBehaviorLog::Destruction(), {"TBTop1Leaf", "TBTop1"});
        ExpectSeq(TransitionBoundaryBehaviorLog::Construction(), {"TBTop2"});
        EXPECT_EQ(machine.counter, 1);
    }
}
