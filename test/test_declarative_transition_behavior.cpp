#include <gtest/gtest.h>
#include <ufsm/ufsm.hpp>

#include "test_support.h"

FSM_EVENT(DTEventToB) {};
FSM_EVENT(DTEventUnrelated) {};
FSM_EVENT(DTEventToSiblingWithAction) {};
FSM_EVENT(DTEventToTop2WithAction) {};

struct DeclarativeTransitionTag;
using DeclarativeTransitionLog = LifecycleLog<DeclarativeTransitionTag>;

#define TRACK_LIFECYCLE(TYPE) \
 public: \
    TYPE() { DeclarativeTransitionLog::RecordConstruction(#TYPE); } \
    ~TYPE() { DeclarativeTransitionLog::RecordDestruction(#TYPE); }

struct DTTop;
struct DTStateA;
struct DTStateB;

struct DTParent;
struct DTLeafA;
struct DTLeafB;

struct DTTop1;
struct DTTop1Leaf;
struct DTTop2;

FSM_STATE_MACHINE(DeclarativeTransitionMachine, DTTop) {};

FSM_STATE(DTTop, DeclarativeTransitionMachine, DTStateA) {
    TRACK_LIFECYCLE(DTTop);
};

// DTEventToB triggers a direct state transition without defining React().
FSM_STATE(DTStateA, DTTop) {
    using reactions = ufsm::List<
        ufsm::Transition<DTEventToB, DTStateB>
    >;

    TRACK_LIFECYCLE(DTStateA);
};

FSM_STATE(DTStateB, DTTop) {
    TRACK_LIFECYCLE(DTStateB);
};

// Action-on-LCA coverage.
FSM_STATE_MACHINE(DeclarativeActionSiblingMachine, DTParent) {
    int action_counter = 0;
};

FSM_STATE(DTParent, DeclarativeActionSiblingMachine, DTLeafA) {
    TRACK_LIFECYCLE(DTParent);
};

struct DTSiblingAction {
    void operator()(DTParent& parent) const {
        parent.OutermostContext().action_counter++;
    }
};


FSM_STATE(DTLeafA, DTParent) {
    using reactions = ufsm::List<
        ufsm::Transition<DTEventToSiblingWithAction, DTLeafB, DTSiblingAction>
    >;

    TRACK_LIFECYCLE(DTLeafA);
};

FSM_STATE(DTLeafB, DTParent) {
    TRACK_LIFECYCLE(DTLeafB);
};

FSM_STATE_MACHINE(DeclarativeActionTopLevelMachine, DTTop1) {
    int action_counter = 0;
};

FSM_STATE(DTTop1, DeclarativeActionTopLevelMachine, DTTop1Leaf) {
    TRACK_LIFECYCLE(DTTop1);
};

struct DTTopLevelAction {
    void operator()(DeclarativeActionTopLevelMachine& machine) const {
        machine.action_counter++;
    }
};


FSM_STATE(DTTop1Leaf, DTTop1) {
    using reactions = ufsm::List<
        ufsm::Transition<DTEventToTop2WithAction, DTTop2, DTTopLevelAction>
    >;

    TRACK_LIFECYCLE(DTTop1Leaf);
};

FSM_STATE(DTTop2, DeclarativeActionTopLevelMachine) {
    TRACK_LIFECYCLE(DTTop2);
};

TEST(DeclarativeTransitionBehaviorTest, TransitionEntryDoesNotRequireReactFunction) {
    DeclarativeTransitionMachine machine;
    machine.Initiate();

    // Ignore initial construction noise.
    DeclarativeTransitionLog::Clear();

    const auto res = machine.ProcessEvent(DTEventToB{});
    EXPECT_EQ(res, ufsm::Result::kConsumed);

    ExpectSeq(DeclarativeTransitionLog::Destruction(), {"DTStateA"});
    ExpectSeq(DeclarativeTransitionLog::Construction(), {"DTStateB"});
}

TEST(DeclarativeTransitionBehaviorTest, UnmatchedEventsStillForwardNormally) {
    DeclarativeTransitionMachine machine;
    machine.Initiate();

    const auto res = machine.ProcessEvent(DTEventUnrelated{});
    EXPECT_EQ(res, ufsm::Result::kForwardEvent);
}

TEST(DeclarativeTransitionBehaviorTest, TransitionEntryWithActionExecutesOnLeastCommonAncestorSibling) {
    DeclarativeActionSiblingMachine machine;
    machine.Initiate();

    DeclarativeTransitionLog::Clear();
    const auto res = machine.ProcessEvent(DTEventToSiblingWithAction{});
    EXPECT_EQ(res, ufsm::Result::kConsumed);
    EXPECT_EQ(machine.action_counter, 1);
}

TEST(DeclarativeTransitionBehaviorTest, TransitionEntryWithActionExecutesOnLeastCommonAncestorTopLevel) {
    DeclarativeActionTopLevelMachine machine;
    machine.Initiate();

    DeclarativeTransitionLog::Clear();
    const auto res = machine.ProcessEvent(DTEventToTop2WithAction{});
    EXPECT_EQ(res, ufsm::Result::kConsumed);
    EXPECT_EQ(machine.action_counter, 1);
}
