#include <ufsm.h>

#include "../cmdline.h"
#include "connector_state_machine.h"

class Connecting;
class Disconnecting;
class Connected;
class Disconnected;

class Disconnected : public Connector {
  Transition React(const EvConnect &event) override {
    Context()++;
    return Transit<Connecting>();
  }
  MARK_CLASS(Disconnected);
};

class Connecting : public Connector {
  Transition React(const EvConnectSuccess &event) override {
    Context()++;
    return Transit<Connected>();
  }
  Transition React(const EvConnectFailure &event) override {
    Context()++;
    return Transit<Disconnected>();
  }
  MARK_CLASS(Connecting);
};

class Connected : public Connector {
 public:
  Connected() {
    MARK_FUNCTION;
    sub_fsm_.Initiate<Working>();
  }
  ~Connected() override { MARK_FUNCTION; }

  Transition React(const EvDisconnect &event) override {
    Context()++;
    return Transit<Disconnecting>();
  }

  Transition React(const EvTick &event) override {
    MARK_FUNCTION;
    // Distribute the event to the sub-state machine for processing.
    sub_fsm_.ProcessEvent(event);
    return NoTransit{};
  }

  class Using : public ufsm::StateBase<Using> {
   public:
    virtual Transition React(const EvTick &event) { return NoTransit{}; }
    MARK_CLASS(Using);
  };

  class Working : public Using {
    Transition React(const EvTick &event) override {
      MARK_FUNCTION;
      return NoTransit{};
    }
    MARK_CLASS(Working);
  };

  ufsm::Fsm<Using> sub_fsm_;
};

class Disconnecting : public Connector {
  Transition React(const EvDisconnectSuccess &event) override {
    Context()++;
    return Transit<Disconnected>();
  }
  MARK_CLASS(Disconnecting);
};

int main() {
  ufsm::Fsm<Connector> connector;
  connector.Initiate<Disconnected>();
  CmdProcessor cmdline;
  cmdline
      .Add("connect", [&]() { connector.ProcessEvent(Connector::EvConnect{}); })
      .Add("disconnect",
           [&]() { connector.ProcessEvent(Connector::EvDisconnect{}); })
      .Add("connect_success",
           [&]() { connector.ProcessEvent(Connector::EvConnectSuccess{}); })
      .Add("connect_failure",
           [&]() { connector.ProcessEvent(Connector::EvConnectFailure{}); })
      .Add("tick", [&]() { connector.ProcessEvent(Connector::EvTick{}); })
      .Add("disconnect_success",
           [&]() { connector.ProcessEvent(Connector::EvDisconnectSuccess{}); });

  LOG << "Command: " << cmdline.DumpCmd() << std::endl;
  std::string str;
  while (std::cin >> str) {
    if (cmdline.ProcessCmd(str)) {
      LOG << "Connector context:" << connector.Context() << std::endl;
    }
    LOG << "Command: " << cmdline.DumpCmd() << std::endl;
  }
}
