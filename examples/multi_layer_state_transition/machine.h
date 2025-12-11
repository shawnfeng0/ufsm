//
// Created by fs on 3/2/21.
//

#pragma once

#include <ufsm/ufsm.hpp>

struct EventA : ufsm::Event<EventA> {};
struct EventB : ufsm::Event<EventB> {};
struct EventC : ufsm::Event<EventC> {};
struct EventD : ufsm::Event<EventD> {};

struct StateA;
struct Machine : ufsm::StateMachine<Machine, StateA> {
  MARK_CLASS(Machine);
};
