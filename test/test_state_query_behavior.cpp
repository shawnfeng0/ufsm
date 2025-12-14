#include <gtest/gtest.h>
#include <ufsm/ufsm.hpp>

FSM_EVENT(SQEvToB) {};

struct SQTop;
struct SQStateA;
struct SQStateB;

FSM_STATE_MACHINE(StateQueryMachine, SQTop) {};

FSM_STATE(SQTop, StateQueryMachine, SQStateA) {};

FSM_STATE(SQStateA, SQTop) {
    using reactions = ufsm::List<
        ufsm::Transition<SQEvToB, SQStateB>
    >;
};

FSM_STATE(SQStateB, SQTop) {};

TEST(StateQueryBehaviorTest, IsInStateAndStatePtrReflectActiveChain) {
    StateQueryMachine machine;

    // Before initiate: no active states.
    EXPECT_EQ(machine.StatePtr<SQTop>(), nullptr);
    EXPECT_FALSE(machine.IsInState<SQTop>());

    machine.Initiate();

    // After initiate: SQTop and SQStateA are active.
    EXPECT_NE(machine.StatePtr<SQTop>(), nullptr);
    EXPECT_NE(machine.StatePtr<SQStateA>(), nullptr);
    EXPECT_EQ(machine.StatePtr<SQStateB>(), nullptr);

    EXPECT_TRUE(machine.IsInState<SQTop>());
    EXPECT_TRUE(machine.IsInState<SQStateA>());
    EXPECT_FALSE(machine.IsInState<SQStateB>());

    machine.ProcessEvent(SQEvToB{});

    // After transition: SQTop and SQStateB are active.
    EXPECT_NE(machine.StatePtr<SQTop>(), nullptr);
    EXPECT_EQ(machine.StatePtr<SQStateA>(), nullptr);
    EXPECT_NE(machine.StatePtr<SQStateB>(), nullptr);

    EXPECT_TRUE(machine.IsInState<SQTop>());
    EXPECT_FALSE(machine.IsInState<SQStateA>());
    EXPECT_TRUE(machine.IsInState<SQStateB>());
}
