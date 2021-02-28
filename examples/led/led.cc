//
// Created by fs on 2021-02-22.
//

#include <ufsm/reaction.h>
#include <ufsm/type_list.h>
#include <ufsm/ufsm.h>

#include <array>
#include <vector>

#include "../log.h"

using namespace ufsm::type;
using namespace std;

struct Event1 : ufsm::Event<Event1> {};
struct Event2 : ufsm::Event<Event2> {};

struct Connected : ufsm::State<Connected> {
  using reactions =
      ufsm::type::List<ufsm::Reaction<Event1>, ufsm::Reaction<Event2>>;
  ufsm::Result React(const Event1 &evt) { return ufsm::detail::consumed; }
  ufsm::Result React(const Event2 &evt) { return ufsm::detail::consumed; }
};

int main(int argc, char *argv[]) {
  LOG << ListLength<List<int, float, void, void *>>::value << std::endl;
  LOG << ListLength<int, float, void, void *>::value << std::endl;

  ufsm::detail::StateBase *base = new Connected();
  base->ReactImpl(Event1{});
  base->ReactImpl(Event2{});

  return 0;
}
