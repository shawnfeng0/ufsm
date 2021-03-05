//
// Created by fs on 3/2/21.
//

#pragma once

#include "machine.h"

struct StateBA;
FSM_STATE_DEFINE(StateB, Machine, StateBA) { MARK_CLASS(StateB); };
FSM_STATE_DEFINE(StateBA, Machine) { MARK_CLASS(StateBA); };
