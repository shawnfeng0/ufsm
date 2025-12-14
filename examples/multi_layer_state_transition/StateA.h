//
// Created by fs on 3/2/21.
//

#pragma once

#include <string>

#include "../log.h"
#include "machine.h"

FSM_STATE(StateA, Machine, StateAA) {
  std::string state_a_context_string;
  MARK_CLASS(StateA);
};

FSM_STATE(StateAA, StateA, StateAAA) { MARK_CLASS(StateAA); };

struct StateABAA;
FSM_STATE(StateAAA, StateAA, StateAAAA) { MARK_CLASS(StateAAA); };
FSM_STATE(StateAAAA, StateAAA) {
  using reactions =
      ufsm::mp::List<ufsm::Reaction<EventA>, ufsm::Reaction<EventB>>;
  ufsm::Result React(const EventA& evt) {
    LOG << evt.Name() << " received in StateAAAA" << std::endl;
    Context<StateA>().state_a_context_string = "set-before-transit";
    MARK_FUNCTION;
    // Transition action runs on the least common ancestor context (here it's StateA).
    return Transit<StateABAA>([](StateA& common_parent) {
      std::cout << "[action] AAAA -> ABAA, common=StateA" << std::endl;
      common_parent.state_a_context_string = "set-by-action";
    });
  }

  ufsm::Result React(const EventB&) {
    MARK_FUNCTION;
    return ufsm::discard_event();
  }

 public:
  StateAAAA() { MARK_FUNCTION; }
  ~StateAAAA() { MARK_FUNCTION; }
};

FSM_STATE(StateAB, StateA, StateABA) { MARK_CLASS(StateAB); };

FSM_STATE(StateABA, StateAB, StateABAA) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EventB>>;
  ufsm::Result React(const EventB&) {
    std::cout << "context(before): " << Context<StateA>().state_a_context_string
              << std::endl;
    MARK_FUNCTION;
    // No-arg action is also supported.
    StateA* p_state_a = &Context<StateA>();
    return Transit<StateAAAA>([p_state_a]() {
      std::cout << "[action] ABA -> AAAA" << std::endl;
      p_state_a->state_a_context_string = "set-by-noarg-action";
    });
  }

  MARK_CLASS(StateABA);
};

FSM_STATE(StateABAA, StateABA, StateABAAA) { MARK_CLASS(StateABAA); };
FSM_STATE(StateABAAA, StateABAA) { MARK_CLASS(StateABAAA); };

FSM_STATE(StateABB, StateAB, StateABBA) { MARK_CLASS(StateABB); };

FSM_STATE(StateABBA, StateABB, StateABBAA) { MARK_CLASS(StateABBA); };
FSM_STATE(StateABBAA, StateABBA) { MARK_CLASS(StateABBAA); };
