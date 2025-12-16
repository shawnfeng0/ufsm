
#include "../cmdline.h"
#include "connector_state_machine.h"

int main() {
  Connector connector;
  connector.Initiate();
  CmdProcessor cmdline;
  cmdline.Add("connect", [&]() { connector.ProcessEvent(EvConnect{}); })
      .Add("disconnect", [&]() { connector.ProcessEvent(EvDisconnect{}); })
      .Add("connect_success", [&]() { connector.ProcessEvent(EvConnectSuccess{}); })
      .Add("connect_failure", [&]() { connector.ProcessEvent(EvConnectFailure{}); })
      .Add("tick", [&]() { connector.ProcessEvent(EvTick{}); })
      .Add("disconnect_success", [&]() { connector.ProcessEvent(EvDisconnectSuccess{}); });

  LOG << "Command: " << cmdline.DumpCmd() << std::endl << "$ ";
  std::string str;
  while (std::cin >> str) {
    if (cmdline.ProcessCmd(str)) {
      //      LOG << "Connector context:" << connector.Context() << std::endl;
    }
    LOG << "Command: " << cmdline.DumpCmd() << std::endl << "$ ";
  }
}
