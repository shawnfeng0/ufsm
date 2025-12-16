#include "ufsm/ufsm.hpp"
#include "test_support.h"

namespace drill_down_test {

struct LogTag {};
using Log = LifecycleLog<LogTag>;

FSM_EVENT(GoToChild){};

struct Parent;
struct Child;

FSM_STATE_MACHINE(Machine, Parent) {
  void OnUnhandledEvent(const ufsm::detail::EventBase& /*event*/) {
    // No-op
  }
};

struct Parent : ufsm::State<Parent, Machine> {
  Parent() { Log::RecordConstruction("Parent"); }
  ~Parent() { Log::RecordDestruction("Parent"); }

  using reactions = ufsm::List<
      ufsm::Transition<GoToChild, Child>
  >;
};

struct Child : ufsm::State<Child, Parent> {
  Child() { Log::RecordConstruction("Child"); }
  ~Child() { Log::RecordDestruction("Child"); }
};

class DrillDownBehaviorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Log::Clear();
  }
};

TEST_F(DrillDownBehaviorTest, ParentRemainsActiveOnDrillDown) {
  Machine fsm;
  fsm.Initiate();

  // Initial state: Parent constructed.
  ExpectSeq(Log::Construction(), {"Parent"});
  ExpectSeq(Log::Destruction(), {});

  Log::Clear();

  // Trigger drill-down transition.
  fsm.ProcessEvent(GoToChild());

  // Expectation:
  // Parent should NOT be destructed or reconstructed.
  // Child should be constructed.
  ExpectSeq(Log::Construction(), {"Child"});
  ExpectSeq(Log::Destruction(), {});

  ASSERT_TRUE(fsm.IsInState<Parent>());
  ASSERT_TRUE(fsm.IsInState<Child>());
}

} // namespace drill_down_test
