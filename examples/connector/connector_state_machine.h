#pragma once

#include <ufsm/aux_macro.h>
#include <ufsm/state.h>
#include <ufsm/state_machine.h>

#include "../log.h"

FSM_EVENT_DEFINE(EvConnect){};
FSM_EVENT_DEFINE(EvConnectSuccess){};
FSM_EVENT_DEFINE(EvConnectFailure){};
FSM_EVENT_DEFINE(EvDisconnect){};
FSM_EVENT_DEFINE(EvDisconnectSuccess){};
FSM_EVENT_DEFINE(EvTick){};

struct Disconnected;
struct Connector : ufsm::StateMachine<Connector, Disconnected> {};

struct Connected;
struct Connecting;
struct Disconnecting;

FSM_STATE_DEFINE(Disconnected, Connector) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EvConnect>>;
  ufsm::Result React(const EvConnect &event) { return Transit<Connecting>(); }
  MARK_CLASS(Disconnected);
};

FSM_STATE_DEFINE(Connecting, Connector) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EvConnectFailure>,
                                   ufsm::Reaction<EvConnectSuccess>>;
  ufsm::Result React(const EvConnectSuccess &event) {
    return Transit<Connected>();
  }
  ufsm::Result React(const EvConnectFailure &event) {
    return Transit<Disconnected>();
  }
  MARK_CLASS(Connecting);
};

struct Working;
FSM_STATE_DEFINE(Connected, Connector, Working) {
  MARK_CLASS(Connected);

  using reactions = ufsm::mp::List<ufsm::Reaction<EvDisconnect>>;
  ufsm::Result React(const EvDisconnect &event) {
    return Transit<Disconnecting>();
  }
};

FSM_STATE_DEFINE(Working, Connected) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EvTick>>;
  ufsm::Result React(const EvTick &event) {
    MARK_FUNCTION;
    return ufsm::detail::consumed;
  }
  MARK_CLASS(Working);
};

FSM_STATE_DEFINE(Disconnecting, Connector) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EvDisconnectSuccess>>;
  ufsm::Result React(const EvDisconnectSuccess &event) {
    return Transit<Disconnected>();
  }
  MARK_CLASS(Disconnecting);
};
