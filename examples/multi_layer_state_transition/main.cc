#include <ufsm/ufsm.hpp>

#include "../log.h"
#include "StateA.h"
#include "StateB.h"
#include "StateC.h"
#include "machine.h"

using namespace ufsm::mp;
using namespace std;

int main(int argc, char *argv[]) {
  Machine machine;

  machine.Initiate();
  machine.ProcessEvent(EventA{});
  machine.ProcessEvent(EventB{});
  machine.ProcessEvent(EventC{});
  machine.ProcessEvent(EventD{});

  std::cout << std::endl;
  machine.Initiate();
  machine.Terminate();

  return 0;
}
