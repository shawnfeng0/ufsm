#include <gtest/gtest.h>
#include <ufsm/ufsm.hpp>

// Events for Context/OutermostContext access tests.
FSM_EVENT(CtxEventToB) {};
FSM_EVENT(CtxEventPingB) {};
FSM_EVENT(CtxEventToTop2) {};


struct CtxParent;
struct CtxLeafA;
struct CtxLeafB;
struct CtxTop2;

FSM_STATE_MACHINE(ContextAccessMachine, CtxParent) {
    // Minimal coverage for ContextPtr<T>() API.
    bool parent_ptr_non_null_in_a = false;
    bool parent_ptr_non_null_in_ping_b = false;
    bool parent_ptr_non_null_in_to_top2 = false;

    // Parent state data propagation.
    int parent_value_seen_in_b = -1;

    // Marker to confirm state code can modify the outermost context.
    int outermost_counter = 0;

    // Signals that we actually reached/handled code in CtxLeafB.
    int ping_b_calls = 0;
    int transit_to_top2_calls = 0;
};

FSM_STATE(CtxParent, ContextAccessMachine, CtxLeafA) {
    int value = 0;
};

FSM_STATE(CtxLeafA, CtxParent) {
    using reactions = ufsm::mp::List<
        ufsm::Reaction<CtxEventToB>
    >;

    ufsm::Result React(const CtxEventToB&) {
        // Access and mutate the parent state via Context<T>().
        auto& parent = Context<CtxParent>();
        parent.value = 42;

        // Also prove we can access and mutate the outermost context.
        OutermostContext().outermost_counter++;

        OutermostContext().parent_ptr_non_null_in_a = (ContextPtr<CtxParent>() != nullptr);

        // Sibling transition under common parent.
        return Transit<CtxLeafB>();
    }

};

FSM_STATE(CtxLeafB, CtxParent) {
    using reactions = ufsm::mp::List<
        ufsm::Reaction<CtxEventPingB>,
        ufsm::Reaction<CtxEventToTop2>
    >;

    ufsm::Result React(const CtxEventPingB&) {
        OutermostContext().ping_b_calls++;
        OutermostContext().parent_ptr_non_null_in_ping_b = (ContextPtr<CtxParent>() != nullptr);
        return ufsm::consume_event();
    }

    ufsm::Result React(const CtxEventToTop2&) {
        // Parent state should still be alive and accessible.
        auto& parent = Context<CtxParent>();
        OutermostContext().parent_value_seen_in_b = parent.value;

        OutermostContext().parent_ptr_non_null_in_to_top2 = (ContextPtr<CtxParent>() != nullptr);

        OutermostContext().transit_to_top2_calls++;

        // Cross top-level transition.
        return Transit<CtxTop2>();
    }

};

FSM_STATE(CtxTop2, ContextAccessMachine) {
};

TEST(ContextAccessBehaviorTest, ContextAccessBehaviorsInAllScenarios) {
    // Scenario 1: sibling transition keeps parent context alive; child B can access it.
    {
        ContextAccessMachine machine;
        machine.Initiate();
        machine.ProcessEvent(CtxEventToB{});

        // Prove we really ended up in CtxLeafB (event handled only there).
        machine.ProcessEvent(CtxEventPingB{});

        EXPECT_EQ(machine.outermost_counter, 1);
        EXPECT_TRUE(machine.parent_ptr_non_null_in_a);
        EXPECT_TRUE(machine.parent_ptr_non_null_in_ping_b);
        EXPECT_EQ(machine.ping_b_calls, 1);
    }

    // Scenario 2: data written in A is visible in B before cross top-level transition.
    {
        ContextAccessMachine machine;
        machine.Initiate();
        machine.ProcessEvent(CtxEventToB{});
        machine.ProcessEvent(CtxEventToTop2{});

        EXPECT_EQ(machine.parent_value_seen_in_b, 42);
        EXPECT_TRUE(machine.parent_ptr_non_null_in_to_top2);
        EXPECT_EQ(machine.transit_to_top2_calls, 1);
    }
}
