#include <ufsm/state_machine.h>
#include <ufsm/state.h>
#include <ufsm/event.h>
#include <iostream>

struct EventA : ufsm::Event<EventA> {};
struct EventB : ufsm::Event<EventB> {};

struct MyMachine;
namespace ufsm {
    template <> struct EventListTrait<MyMachine> {
        using type = ufsm::mp::List<EventA, EventB>;
    };
}

struct State1;
struct State2;

struct MyMachine : ufsm::StateMachine<MyMachine, State1> {
};

struct State1 : ufsm::State<State1, MyMachine> {
    void React(const EventA&) {
        std::cout << "State1 handled EventA" << std::endl;
        Transit<State2>();
    }
    // No reactions typedef!
};

struct State2 : ufsm::State<State2, MyMachine> {
    void React(const EventB&) {
        std::cout << "State2 handled EventB" << std::endl;
    }
};

int main() {
    MyMachine sm;

    sm.Initiate();
    std::cout << "Sending EventA..." << std::endl;
    sm.ProcessEvent(EventA());
    std::cout << "Sending EventB..." << std::endl;
    sm.ProcessEvent(EventB());
    return 0;
}
