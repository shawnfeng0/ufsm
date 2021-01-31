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
  Transition React(const EvDisconnect &event) override {
    Context()++;
    return Transit<Disconnecting>();
  }

  MARK_CLASS(Connected);
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
