#include "ufsm/ufsm.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

namespace {

std::vector<std::string> history;

struct EventA : ufsm::Event<EventA> {};

struct StateA;
struct StateB;

struct Machine : ufsm::StateMachine<Machine, StateA> {
    void OnUnhandledEvent(const ufsm::detail::EventBase&) {}
};

struct StateA : ufsm::State<StateA, Machine> {
    void OnEntry() {
        history.push_back("StateA::OnEntry");
        if (OutermostContext().IsInState<StateA>()) {
            history.push_back("StateA::IsInState=true");
        } else {
            history.push_back("StateA::IsInState=false");
        }
    }
    void OnExit() {
        history.push_back("StateA::OnExit");
        if (OutermostContext().IsInState<StateA>()) {
            history.push_back("StateA::IsInState=true");
        } else {
            history.push_back("StateA::IsInState=false");
        }
    }
    using reactions = ufsm::List<
        ufsm::Transition<EventA, StateB>
    >;
};

struct StateB : ufsm::State<StateB, Machine> {
    void OnEntry() { history.push_back("StateB::OnEntry"); }
    void OnExit() { history.push_back("StateB::OnExit"); }
};

TEST(EntryExitTest, CallsOnEntryAndOnExit) {
    history.clear();
    Machine fsm;
    fsm.Initiate();

    // Should have entered StateA
    // Since OnEntry is not yet implemented, history should be empty or contain constructor logs if we had them.
    // But we expect "StateA::OnEntry" after implementation.

    fsm.ProcessEvent(EventA{});

    // Should have exited StateA and entered StateB

    fsm.Terminate();

    // Should have exited StateB

    std::vector<std::string> expected = {
        "StateA::OnEntry",
        "StateA::IsInState=true",
        "StateA::OnExit",
        "StateA::IsInState=true",
        "StateB::OnEntry",
        "StateB::OnExit"
    };

    EXPECT_EQ(history, expected);
}

} // namespace
