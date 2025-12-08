#pragma once

#include <ufsm/event.h>
#include <ufsm/state.h>
#include <ufsm/state_machine.h>

#define FSM_STATE_MACHINE(StateName, InitialState) \
  struct InitialState;                             \
  struct StateName : ufsm::StateMachine<StateName, InitialState>

#define _FSM_STATE_2(StateName, Context) \
  struct Context;                        \
  struct StateName : ufsm::State<StateName, Context>

#define _FSM_STATE_3(StateName, Context, InitialState) \
  struct Context;                                      \
  struct InitialState;                                 \
  struct StateName : ufsm::State<StateName, Context, InitialState>

#define _FSM_GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define FSM_STATE(...) _FSM_GET_MACRO(__VA_ARGS__, _FSM_STATE_3, _FSM_STATE_2)(__VA_ARGS__)

#define FSM_EVENT(EventName) struct EventName : ufsm::Event<EventName>
