#include <cstring>
#include <iostream>

#include "cmdline.h"
#include "ufsm.h"

// Precompiler define to get only filename;
#if !defined(__FILENAME__)
#define __FILENAME__                                       \
  (strrchr(__FILE__, '/')    ? strrchr(__FILE__, '/') + 1  \
   : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 \
                             : __FILE__)
#endif

#define LOG std::cout << "(" << __FILENAME__ << ":" << __LINE__ << ") "

#define MARK_FUNCTION LOG << __PRETTY_FUNCTION__ << std::endl

#define MARK(TYPE)          \
 public:                    \
  TYPE() { MARK_FUNCTION; } \
  ~TYPE() { MARK_FUNCTION; }

struct ConnectorContext {
  int data;
};

class Connector : public ufsm::State<Connector, ConnectorContext> {
 public:
  struct EvConnect : public ufsm::Event {
    MARK(EvConnect)
  };
  virtual ufsm::Transition React(const EvConnect &event) {
    MARK_FUNCTION;
    return ufsm::NoTransit{};
  }

  struct EvConnectSuccess : public ufsm::Event {
    MARK(EvConnectSuccess)
  };
  virtual ufsm::Transition React(const EvConnectSuccess &event) {
    MARK_FUNCTION;
    return ufsm::NoTransit{};
  }

  struct EvConnectFailure : public ufsm::Event {
    MARK(EvConnectFailure)
  };
  virtual ufsm::Transition React(const EvConnectFailure &event) {
    MARK_FUNCTION;
    return ufsm::NoTransit{};
  }

  struct EvDisconnect : public ufsm::Event {
    MARK(EvDisconnect)
  };
  virtual ufsm::Transition React(const EvDisconnect &event) {
    MARK_FUNCTION;
    return ufsm::NoTransit{};
  }

  struct EvDisconnectSuccess : public ufsm::Event {
    MARK(EvDisconnectSuccess)
  };
  virtual ufsm::Transition React(const EvDisconnectSuccess &event) {
    MARK_FUNCTION;
    return ufsm::NoTransit{};
  }
};

class Connecting;
class Disconnecting;
class Connected;
class Disconnected;

class Disconnected : public Connector {
  ufsm::Transition React(const EvConnect &event) override {
    Context().data++;
    return Transit<Connecting>();
  }
  MARK(Disconnected);
};

class Connecting : public Connector {
  ufsm::Transition React(const EvConnectSuccess &event) override {
    Context().data++;
    return Transit<Connected>();
  }
  ufsm::Transition React(const EvConnectFailure &event) override {
    Context().data++;
    return Transit<Disconnected>();
  }
  MARK(Connecting);
};

class Connected : public Connector {
  ufsm::Transition React(const EvDisconnect &event) override {
    Context().data++;
    return Transit<Disconnecting>();
  }
  MARK(Connected);
};

class Disconnecting : public Connector {
  ufsm::Transition React(const EvDisconnect &event) override {
    Context().data++;
    return Transit<Disconnected>();
  }
  MARK(Disconnecting);
};

int main() {
  ufsm::Fsm<Connector> connector;
  connector.Initiate<Disconnected>();
  Cmdline cmdline;
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
  std::cout << cmdline.Dump();

  std::string str;
  while (std::cin >> str) {
    if (cmdline.ProcessCmd(str)) {
      std::cout << "Command executed." << std::endl;
      LOG << connector.Context().data << std::endl;
    } else {
      std::cout << cmdline.Dump();
    }
  }
}
