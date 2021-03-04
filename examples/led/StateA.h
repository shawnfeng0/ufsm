//
// Created by fs on 3/2/21.
//

#pragma once

#include <ufsm/aux_macro.h>

#include "TestMachine.h"

struct StateAA;
FSM_STATE_DEFINE(StateA, TestMachine, StateAA) {
  std::string state_a_context_string;
  MARK_CLASS(StateA);
};

struct StateAAA;
FSM_STATE_DEFINE(StateAA, StateA, StateAAA) { MARK_CLASS(StateAA); };

// struct StateAAAA;
struct StateAAAA;
struct StateABAA;
FSM_STATE_DEFINE(StateAAA, StateAA, StateAAAA) { MARK_CLASS(StateAAA); };
FSM_STATE_DEFINE(StateAAAA, StateAAA) {
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

struct StateABA;
FSM_STATE_DEFINE(StateAB, StateA, StateABA) { MARK_CLASS(StateAB); };

struct StateABAA;
FSM_STATE_DEFINE(StateABA, StateAB, StateABAA) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EventB>>;
  ufsm::Result React(const EventB& event) {
    std::cout << "context: " << Context<StateA>().state_a_context_string
              << std::endl;
    MARK_FUNCTION;
    return Transit<StateAAAA>();
  }

  MARK_CLASS(StateABA);
};

struct StateABAAA;
FSM_STATE_DEFINE(StateABAA, StateABA, StateABAAA) { MARK_CLASS(StateABAA); };
FSM_STATE_DEFINE(StateABAAA, StateABAA) { MARK_CLASS(StateABAAA); };

struct StateABBA;
FSM_STATE_DEFINE(StateABB, StateAB, StateABBA) { MARK_CLASS(StateABB); };

struct StateABBAA;
FSM_STATE_DEFINE(StateABBA, StateABB, StateABBAA) { MARK_CLASS(StateABBA); };
FSM_STATE_DEFINE(StateABBAA, StateABBA) { MARK_CLASS(StateABBAA); };
