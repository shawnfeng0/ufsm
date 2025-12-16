#include <gtest/gtest.h>

#include <ufsm/ufsm.hpp>

#include "test_support.h"

// Events used by routing behavior tests.
FSM_EVENT(EREventForwardToParent){};
FSM_EVENT(EREventDiscardAtChild){};

struct EventRoutingBehaviorTag;
using EventRoutingBehaviorLog = LifecycleLog<EventRoutingBehaviorTag>;

#define TRACK_LIFECYCLE(TYPE)                                    \
 public:                                                         \
  TYPE() { EventRoutingBehaviorLog::RecordConstruction(#TYPE); } \
  ~TYPE() { EventRoutingBehaviorLog::RecordDestruction(#TYPE); }

struct ERChild;

FSM_STATE_MACHINE(EventRoutingMachine, ERParent) {
  int child_forward_called = 0;
  int child_discard_called = 0;
  int parent_called = 0;
};

// Parent -> Child
FSM_STATE(ERParent, EventRoutingMachine, ERChild) {
  using reactions = ufsm::List<ufsm::Reaction<EREventForwardToParent>, ufsm::Reaction<EREventDiscardAtChild> >;

  ufsm::Result React(const EREventForwardToParent&) {
    OutermostContext().parent_called++;
    return ConsumeEvent();
  }

  ufsm::Result React(const EREventDiscardAtChild&) {
    // If the child returns do_discard_event correctly, the parent must not see this.
    OutermostContext().parent_called++;
    return ConsumeEvent();
  }

  TRACK_LIFECYCLE(ERParent);
};

FSM_STATE(ERChild, ERParent) {
  using reactions = ufsm::List<ufsm::Reaction<EREventForwardToParent>, ufsm::Reaction<EREventDiscardAtChild> >;

  ufsm::Result React(const EREventForwardToParent&) {
    OutermostContext().child_forward_called++;
    return ForwardEvent();
  }

  ufsm::Result React(const EREventDiscardAtChild&) {
    OutermostContext().child_discard_called++;
    return DiscardEvent();
  }

  TRACK_LIFECYCLE(ERChild);
};

namespace {

void ExpectEmptyLifecycleDelta() {
  EXPECT_TRUE(EventRoutingBehaviorLog::Construction().empty());
  EXPECT_TRUE(EventRoutingBehaviorLog::Destruction().empty());
}

}  // namespace

TEST(EventRoutingBehaviorTest, ForwardAndDiscardRoutingBehaviors) {
  // Scenario 1: Forwarding should call the child handler once, then propagate to the parent.
  {
    EventRoutingMachine machine;
    machine.Initiate();

    EventRoutingBehaviorLog::Clear();
    const auto res = machine.ProcessEvent(EREventForwardToParent{});
    EXPECT_EQ(res, ufsm::Result::kConsumed);

    ExpectEmptyLifecycleDelta();
    EXPECT_EQ(machine.child_forward_called, 1);
    EXPECT_EQ(machine.child_discard_called, 0);
    EXPECT_EQ(machine.parent_called, 1);
  }

  // Scenario 2: Discarding should be handled at the child and must not propagate to the parent.
  {
    EventRoutingMachine machine;
    machine.Initiate();

    EventRoutingBehaviorLog::Clear();
    const auto res = machine.ProcessEvent(EREventDiscardAtChild{});
    EXPECT_EQ(res, ufsm::Result::kDiscardEvent);

    ExpectEmptyLifecycleDelta();
    EXPECT_EQ(machine.child_forward_called, 0);
    EXPECT_EQ(machine.child_discard_called, 1);
    EXPECT_EQ(machine.parent_called, 0);
  }
}
