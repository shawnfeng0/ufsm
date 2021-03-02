//
// Created by fs on 3/2/21.
//

#pragma once

#include "TestMachine.h"

struct StateBA;
FSM_STATE_DEFINE(StateB, TestMachine, StateBA) { MARK_CLASS(StateB); };
FSM_STATE_DEFINE(StateBA, TestMachine) { MARK_CLASS(StateBA); };
