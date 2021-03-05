//
// Created by fs on 3/2/21.
//

#pragma once

#include "machine.h"

struct StateBA;
FSM_STATE(StateB, Machine, StateBA) { MARK_CLASS(StateB); };
FSM_STATE(StateBA, Machine) { MARK_CLASS(StateBA); };
