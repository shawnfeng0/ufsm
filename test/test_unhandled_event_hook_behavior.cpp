#include <gtest/gtest.h>
#include <ufsm/ufsm.hpp>

FSM_EVENT(UHEvHandled) {};
FSM_EVENT(UHEvUnhandled) {};

struct UHTop;
struct UHLeaf;

FSM_STATE_MACHINE(UnhandledHookMachine, UHTop) {
    int unhandled_count = 0;

    // Optional hook: called only when the event is not handled anywhere.
    void OnUnhandledEvent(const ufsm::detail::EventBase&) {
        unhandled_count++;
    }
};

FSM_STATE(UHTop, UnhandledHookMachine, UHLeaf) {};

FSM_STATE(UHLeaf, UHTop) {
    using reactions = ufsm::List<
        ufsm::Reaction<UHEvHandled>
    >;

    ufsm::Result React(const UHEvHandled&) {
        return ufsm::consume_event();
    }
};

TEST(UnhandledEventHookBehaviorTest, HookRunsOnlyForUnhandledEvents) {
    UnhandledHookMachine machine;
    machine.Initiate();

    EXPECT_EQ(machine.unhandled_count, 0);

    // Unhandled: no matching reaction anywhere.
    EXPECT_EQ(machine.ProcessEvent(UHEvUnhandled{}), ufsm::Result::kForwardEvent);
    EXPECT_EQ(machine.unhandled_count, 1);

    // Handled by leaf.
    EXPECT_EQ(machine.ProcessEvent(UHEvHandled{}), ufsm::Result::kConsumed);
    EXPECT_EQ(machine.unhandled_count, 1);
}
