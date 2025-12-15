#include <gtest/gtest.h>
#include <ufsm/ufsm.hpp>

#include "test_support.h"

#include <vector>

FSM_EVENT(IEventHandled) {};

struct IdempotencyBehaviorTag;
using IdempotencyBehaviorLog = LifecycleLog<IdempotencyBehaviorTag>;

#define TRACK_LIFECYCLE(TYPE) \
 public: \
    TYPE() { IdempotencyBehaviorLog::RecordConstruction(#TYPE); } \
    ~TYPE() { IdempotencyBehaviorLog::RecordDestruction(#TYPE); }

struct ILeaf;

FSM_STATE_MACHINE(IdempotencyMachine, IParent) {
    int handled_calls = 0;
};

FSM_STATE(IParent, IdempotencyMachine, ILeaf) {
    TRACK_LIFECYCLE(IParent);
};

FSM_STATE(ILeaf, IParent) {
    using reactions = ufsm::List<
        ufsm::Reaction<IEventHandled>
    >;

    ufsm::Result React(const IEventHandled&) {
        OutermostContext().handled_calls++;
        return consume_event();
    }

    TRACK_LIFECYCLE(ILeaf);
};

TEST(IdempotencyBehaviorTest, IdempotencyAndPostTerminateBehaviors) {
    // Scenario 1: Terminate is idempotent.
    {
        IdempotencyMachine machine;
        machine.Initiate();

        IdempotencyBehaviorLog::Clear();
        machine.Terminate();
        ExpectSeq(IdempotencyBehaviorLog::Destruction(), {"ILeaf", "IParent"});

        // Second terminate must do nothing.
        IdempotencyBehaviorLog::Clear();
        machine.Terminate();
        EXPECT_TRUE(IdempotencyBehaviorLog::Construction().empty());
        EXPECT_TRUE(IdempotencyBehaviorLog::Destruction().empty());
    }

    // Scenario 2: Initiate resets the active hierarchy.
    {
        IdempotencyMachine machine;
        machine.Initiate();
        machine.ProcessEvent(IEventHandled{});
        EXPECT_EQ(machine.handled_calls, 1);

        // Re-initiate must terminate the previous hierarchy and construct a fresh one.
        IdempotencyBehaviorLog::Clear();
        machine.Initiate();

        // Current implementation performs full termination before constructing again.
        // We validate that both destruction and construction happened in this call.
        ExpectSeq(IdempotencyBehaviorLog::Destruction(), {"ILeaf", "IParent"});
        ExpectSeq(IdempotencyBehaviorLog::Construction(), {"IParent", "ILeaf"});

        machine.ProcessEvent(IEventHandled{});
        EXPECT_EQ(machine.handled_calls, 2);
    }

    // Scenario 3: ProcessEvent after Terminate has no side effects.
    {
        IdempotencyMachine machine;
        machine.Initiate();
        machine.Terminate();

        IdempotencyBehaviorLog::Clear();

        // After termination, sending events must not crash and must not modify lifecycle.
        const auto res = machine.ProcessEvent(IEventHandled{});
        EXPECT_EQ(res, ufsm::Result::kForwardEvent);

        EXPECT_TRUE(IdempotencyBehaviorLog::Construction().empty());
        EXPECT_TRUE(IdempotencyBehaviorLog::Destruction().empty());
        EXPECT_EQ(machine.handled_calls, 0);
    }
}
