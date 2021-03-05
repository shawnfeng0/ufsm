#pragma once

#include <ufsm/aux_macro.h>
#include <ufsm/state.h>
#include <ufsm/state_machine.h>

#include "../log.h"

FSM_EVENT(EvConnect){};
FSM_EVENT(EvConnectSuccess){};
FSM_EVENT(EvConnectFailure){};
FSM_EVENT(EvDisconnect){};
FSM_EVENT(EvDisconnectSuccess){};
FSM_EVENT(EvTick){};

struct Disconnected;
FSM_STATE_MACHINE(Connector, Disconnected){};

struct Connected;
struct Connecting;
struct Disconnecting;

FSM_STATE(Disconnected, Connector) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EvConnect>>;
  ufsm::Result React(const EvConnect &event) { return Transit<Connecting>(); }
  MARK_CLASS(Disconnected);
};

FSM_STATE(Connecting, Connector) {
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
FSM_STATE(Connected, Connector, Working) {
  MARK_CLASS(Connected);

  using reactions = ufsm::mp::List<ufsm::Reaction<EvDisconnect>>;
  ufsm::Result React(const EvDisconnect &event) {
    return Transit<Disconnecting>();
  }
};

FSM_STATE(Working, Connected) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EvTick>>;
  ufsm::Result React(const EvTick &event) {
    MARK_FUNCTION;
    return ufsm::detail::consumed;
  }
  MARK_CLASS(Working);
};

FSM_STATE(Disconnecting, Connector) {
  using reactions = ufsm::mp::List<ufsm::Reaction<EvDisconnectSuccess>>;
  ufsm::Result React(const EvDisconnectSuccess &event) {
    return Transit<Disconnected>();
  }
  MARK_CLASS(Disconnecting);
};
