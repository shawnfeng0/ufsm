#pragma once

#include "../log.h"

class Connector : public ufsm::StateBase<Connector> {
 public:
  struct EvConnect {};
  virtual Transition React(const EvConnect &) { return NoTransit{}; }

  struct EvConnectSuccess {};
  virtual Transition React(const EvConnectSuccess &) { return NoTransit{}; }

  struct EvConnectFailure {};
  virtual Transition React(const EvConnectFailure &) { return NoTransit{}; }

  struct EvDisconnect {};
  virtual Transition React(const EvDisconnect &) { return NoTransit{}; }

  struct EvDisconnectSuccess {};
  virtual Transition React(const EvDisconnectSuccess &) { return NoTransit{}; }

  struct EvTick {};
  virtual Transition React(const EvTick &) { return NoTransit{}; }
};
