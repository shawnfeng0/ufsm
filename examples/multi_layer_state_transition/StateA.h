//
// Created by fs on 3/2/21.
//

#pragma once

#include <ufsm/macros.h>

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
  ufsm::Result React(const EventA& event) {
    Context<StateA>().state_a_context_string = "test";
    MARK_FUNCTION;
    return Transit<StateABAA>();
  }

  ufsm::Result React(const EventB& event) {
    MARK_FUNCTION;
    return ufsm::Result::consumed;
  }

 public:
  StateAAAA() { MARK_FUNCTION; }
  ~StateAAAA() { MARK_FUNCTION; }
};

FSM_STATE(StateAB, StateA, StateABA) { MARK_CLASS(StateAB); };

FSM_STATE(StateABA, StateAB, StateABAA) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EventB>>;
  ufsm::Result React(const EventB& event) {
    std::cout << "context: " << Context<StateA>().state_a_context_string
              << std::endl;
    MARK_FUNCTION;
    return Transit<StateAAAA>();
  }

  MARK_CLASS(StateABA);
};

FSM_STATE(StateABAA, StateABA, StateABAAA) { MARK_CLASS(StateABAA); };
FSM_STATE(StateABAAA, StateABAA) { MARK_CLASS(StateABAAA); };

FSM_STATE(StateABB, StateAB, StateABBA) { MARK_CLASS(StateABB); };

FSM_STATE(StateABBA, StateABB, StateABBAA) { MARK_CLASS(StateABBA); };
FSM_STATE(StateABBAA, StateABBA) { MARK_CLASS(StateABBAA); };
