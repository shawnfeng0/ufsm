#include <ufsm/macros.h>
#include <ufsm/state.h>
#include <ufsm/state_machine.h>
#include <ufsm/type/list.h>

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

  return 0;
}
