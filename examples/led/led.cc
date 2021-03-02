#include <ufsm/aux_macro.h>
#include <ufsm/state.h>
#include <ufsm/state_machine.h>
#include <ufsm/type/list.h>

#include "../log.h"
#include "StateA.h"
#include "StateB.h"
#include "StateC.h"
#include "TestMachine.h"

using namespace ufsm::type;
using namespace std;

int main(int argc, char *argv[]) {
  LOG << Size<List<int, float, void, void *>>::value << std::endl;
  LOG << Size<int, float, void, void *>::value << std::endl;
  LOG << IsList<List<int, float>>::value << std::endl;
  LOG << IsList<List<>>::value << std::endl;
  LOG << IsList<int, float>::value << std::endl;

  TestMachine machine;

  machine.Initiate();
  machine.ProcessEvent(EventA{});

  return 0;
}
