#include <gtest/gtest.h>

#include <ufsm/ufsm.hpp>

FSM_EVENT(DEvFirst){};
FSM_EVENT(DEvSecond){};
FSM_EVENT(DEvNoop){};

struct DQTop;
struct DQStateA;
struct DQStateB;

FSM_STATE_MACHINE(DeferredQueueMachine, DQTop) { int second_seen = 0; };

FSM_STATE(DQTop, DeferredQueueMachine, DQStateA){};

FSM_STATE(DQStateA, DQTop) {
  using reactions = ufsm::List<ufsm::Reaction<DEvFirst> >;

  ufsm::Result React(const DEvFirst&) {
    // Post a follow-up event instead of processing it re-entrantly.
    PostEvent(DEvSecond{});
    return Transit<DQStateB>();
  }
};

FSM_STATE(DQStateB, DQTop) {
  using reactions = ufsm::List<ufsm::Reaction<DEvSecond> >;

  ufsm::Result React(const DEvSecond&) {
    OutermostContext().second_seen++;
    return ConsumeEvent();
  }
};

TEST(DeferredEventQueueBehaviorTest, PostEventIsDispatchedBeforeProcessEventReturns) {
  DeferredQueueMachine machine;
  machine.Initiate();

  machine.ProcessEvent(DEvFirst{});

  // Boost-like: posted events are processed after the current event completes.
  EXPECT_EQ(machine.second_seen, 1);
}

TEST(DeferredEventQueueBehaviorTest, ExternallyPostedEventsDrainOnNextProcessEvent) {
  DeferredQueueMachine machine;
  machine.Initiate();

  // Move to B first.
  machine.ProcessEvent(DEvFirst{});
  EXPECT_EQ(machine.second_seen, 1);

  machine.PostEvent(DEvSecond{});

  // No manual dispatch API: posted events are processed when the machine is driven
  // by ProcessEvent (Boost-like).
  (void)machine.ProcessEvent(DEvNoop{});
  EXPECT_EQ(machine.second_seen, 2);
}
