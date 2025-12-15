#pragma once

#include <ufsm/ufsm.hpp>

#include "../log.h"

FSM_EVENT(EvConnect){};
FSM_EVENT(EvConnectSuccess){};
FSM_EVENT(EvConnectFailure){};
FSM_EVENT(EvDisconnect){};
FSM_EVENT(EvDisconnectSuccess){};
FSM_EVENT(EvTick){};

FSM_STATE_MACHINE(Connector, Disconnected){};

struct Connected;
struct Connecting;
struct Disconnecting;

FSM_STATE(Disconnected, Connector) {
  using reactions = ufsm::List<ufsm::Reaction<EvConnect>>;
  ufsm::Result React(const EvConnect &) { return Transit<Connecting>(); }
  MARK_CLASS(Disconnected);
};

FSM_STATE(Connecting, Connector) {
  using reactions = ufsm::List<ufsm::Reaction<EvConnectFailure>,
                                   ufsm::Reaction<EvConnectSuccess>>;
  ufsm::Result React(const EvConnectSuccess &) {
    return Transit<Connected>();
  }
  ufsm::Result React(const EvConnectFailure &) {
    return Transit<Disconnected>();
  }
  MARK_CLASS(Connecting);
};

FSM_STATE(Connected, Connector, Working) {
  MARK_CLASS(Connected);

  using reactions = ufsm::List<ufsm::Reaction<EvDisconnect>>;
  ufsm::Result React(const EvDisconnect &) {
    return Transit<Disconnecting>();
  }
};

FSM_STATE(Working, Connected) {
  using reactions = ufsm::List<ufsm::Reaction<EvTick>>;
  ufsm::Result React(const EvTick &) {
    MARK_FUNCTION;
    return discard_event();
  }
  MARK_CLASS(Working);
};

FSM_STATE(Disconnecting, Connector) {
  using reactions = ufsm::List<ufsm::Reaction<EvDisconnectSuccess>>;
  ufsm::Result React(const EvDisconnectSuccess &) {
    return Transit<Disconnected>();
  }
  MARK_CLASS(Disconnecting);
};
