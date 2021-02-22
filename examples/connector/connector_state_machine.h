#pragma once

#include "../log.h"

class Connector : public ufsm::StateBase<int> {
 public:
  struct EvConnect {};
  virtual ufsm::Transition React(const EvConnect &) {
    return ufsm::NoTransit{};
  }

  struct EvConnectSuccess {};
  virtual ufsm::Transition React(const EvConnectSuccess &) {
    return ufsm::NoTransit{};
  }

  struct EvConnectFailure {};
  virtual ufsm::Transition React(const EvConnectFailure &) {
    return ufsm::NoTransit{};
  }

  struct EvDisconnect {};
  virtual ufsm::Transition React(const EvDisconnect &) {
    return ufsm::NoTransit{};
  }

  struct EvDisconnectSuccess {};
  virtual ufsm::Transition React(const EvDisconnectSuccess &) {
    return ufsm::NoTransit{};
  }

  struct EvTick {};
  virtual ufsm::Transition React(const EvTick &) { return ufsm::NoTransit{}; }
};
