//
// Created by fs on 3/2/21.
//

#pragma once

#include <ufsm/state.h>
#include <ufsm/state_machine.h>

#define FSM_STATE(StateName, Context, ...) \
  struct StateName : ufsm::State<StateName, Context, ##__VA_ARGS__>

#define FSM_EVENT(EventName) struct EventName : ufsm::Event<EventName>

#define FSM_STATE_MACHINE(StateName, InitialState) \
  struct StateName : ufsm::StateMachine<StateName, InitialState>
