//
// Created by fs on 2021-02-22.
//

#include <ufsm/state.h>
#include <ufsm/state_machine.h>
#include <ufsm/type/list.h>

#include "../log.h"

using namespace ufsm::type;
using namespace std;

struct StateA;
struct TestMachine : ufsm::StateMachine<TestMachine, StateA> {
  MARK_CLASS(TestMachine);
};

struct StateAA;
struct StateA : ufsm::State<StateA, TestMachine, StateAA> {
  MARK_CLASS(StateA);
};

struct StateAA : ufsm::State<StateAA, StateA> {
  MARK_CLASS(StateAA);
};

int main(int argc, char *argv[]) {
  LOG << Size<List<int, float, void, void *>>::value << std::endl;
  LOG << Size<int, float, void, void *>::value << std::endl;
  LOG << IsList<List<int, float>>::value << std::endl;
  LOG << IsList<List<>>::value << std::endl;
  LOG << IsList<int, float>::value << std::endl;

  TestMachine machine;

  machine.Initiate();

  return 0;
}
