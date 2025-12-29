// Test for TransitWithArgs functionality
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>
#include "ufsm/ufsm.h"

namespace {

// ============================================================================
// Test 1: Basic usage - args-only constructor
// ============================================================================
namespace basic_test {

std::vector<std::string> log;

FSM_EVENT(EvStart) {
  std::string target;
  int priority;
  EvStart(std::string t, int p) : target(std::move(t)), priority(p) {}
};
FSM_EVENT(EvStop){};

struct Idle;
struct Running;

FSM_STATE_MACHINE(Robot, Idle) {
  void OnUnhandledEvent(const ufsm::detail::EventBase&) {}
};

FSM_STATE(Idle, Robot) {
  Idle() { log.push_back("Idle::ctor"); }
  ~Idle() { log.push_back("Idle::dtor"); }

  ufsm::Result React(const EvStart& e) {
    // Use Transit with nullptr action to pass arguments to Running's constructor
    return Transit<Running>(ufsm::with_args, e.target, e.priority);
  }

  using reactions = ufsm::List<ufsm::Reaction<EvStart>>;
};

FSM_STATE(Running, Robot) {
  std::string target_;
  int priority_;

  // Constructor that accepts arguments (no context)
  Running(std::string target, int priority)
      : target_(std::move(target)), priority_(priority) {
    log.push_back("Running::ctor(" + target_ + "," + std::to_string(priority_) + ")");
  }

  ~Running() { log.push_back("Running::dtor"); }

  ufsm::Result React(const EvStop&) {
    return Transit<Idle>();
  }

  using reactions = ufsm::List<ufsm::Reaction<EvStop>>;
};

}  // namespace basic_test

TEST(TransitWithArgsTest, BasicUsageWithArgsOnlyConstructor) {
  using namespace basic_test;
  log.clear();

  Robot robot;
  robot.Initiate();
  EXPECT_EQ(log.back(), "Idle::ctor");

  robot.ProcessEvent(EvStart{"destination_A", 5});

  // Check that Running was constructed with the correct arguments
  EXPECT_TRUE(robot.IsInState<Running>());
  EXPECT_EQ(log.back(), "Running::ctor(destination_A,5)");

  robot.ProcessEvent(EvStop{});
  EXPECT_TRUE(robot.IsInState<Idle>());
}

// ============================================================================
// Test 2: Usage with context + args constructor
// ============================================================================
namespace context_args_test {

std::vector<std::string> log;

FSM_EVENT(EvGo) {
  int value;
  explicit EvGo(int v) : value(v) {}
};

struct StateA;
struct StateB;

FSM_STATE_MACHINE(Machine, StateA) {
  int machine_id = 42;
};

FSM_STATE(StateA, Machine) {
  StateA() { log.push_back("StateA::ctor"); }

  ufsm::Result React(const EvGo& e) {
    return Transit<StateB>(ufsm::with_args, e.value, "extra_info");
  }

  using reactions = ufsm::List<ufsm::Reaction<EvGo>>;
};

FSM_STATE(StateB, Machine) {
  int value_;
  std::string info_;

  // Constructor that accepts context pointer + additional arguments
  StateB(Machine* ctx, int value, std::string info)
      : value_(value), info_(std::move(info)) {
    // Can access context immediately
    log.push_back("StateB::ctor(ctx=" + std::to_string(ctx->machine_id) +
                  ",value=" + std::to_string(value_) +
                  ",info=" + info_ + ")");
  }

  using reactions = ufsm::List<>;
};

}  // namespace context_args_test

TEST(TransitWithArgsTest, UsageWithContextAndArgsConstructor) {
  using namespace context_args_test;
  log.clear();

  Machine m;
  m.Initiate();
  EXPECT_EQ(log.back(), "StateA::ctor");

  m.ProcessEvent(EvGo{100});

  EXPECT_TRUE(m.IsInState<StateB>());
  EXPECT_EQ(log.back(), "StateB::ctor(ctx=42,value=100,info=extra_info)");
}

// ============================================================================
// Test 3: TransitWithArgs in hierarchical state machine
// ============================================================================
namespace hierarchy_test {

std::vector<std::string> log;

FSM_EVENT(EvActivate) {
  std::string mode;
  explicit EvActivate(std::string m) : mode(std::move(m)) {}
};

struct Inactive;
struct Active;
struct Working;

FSM_STATE_MACHINE(HierMachine, Inactive) {};

FSM_STATE(Inactive, HierMachine) {
  Inactive() { log.push_back("Inactive::ctor"); }
  ~Inactive() { log.push_back("Inactive::dtor"); }

  ufsm::Result React(const EvActivate& e) {
    // Transit to a nested state with arguments
    return Transit<Working>(ufsm::with_args, e.mode);
  }

  using reactions = ufsm::List<ufsm::Reaction<EvActivate>>;
};

// Active is a composite state with Working as inner initial
FSM_STATE(Active, HierMachine, Working) {
  Active() { log.push_back("Active::ctor"); }
  ~Active() { log.push_back("Active::dtor"); }
};

FSM_STATE(Working, Active) {
  std::string mode_;

  explicit Working(std::string mode) : mode_(std::move(mode)) {
    log.push_back("Working::ctor(mode=" + mode_ + ")");
  }
  ~Working() { log.push_back("Working::dtor"); }

  using reactions = ufsm::List<>;
};

}  // namespace hierarchy_test

TEST(TransitWithArgsTest, WorksWithHierarchicalStates) {
  using namespace hierarchy_test;
  log.clear();

  HierMachine m;
  m.Initiate();
  EXPECT_TRUE(m.IsInState<Inactive>());

  m.ProcessEvent(EvActivate{"turbo"});

  EXPECT_TRUE(m.IsInState<Active>());
  EXPECT_TRUE(m.IsInState<Working>());

  // Verify the construction order: Active first, then Working with args
  auto it = std::find(log.begin(), log.end(), "Active::ctor");
  ASSERT_NE(it, log.end());
  ++it;
  ASSERT_NE(it, log.end());
  EXPECT_EQ(*it, "Working::ctor(mode=turbo)");
}

// ============================================================================
// Test 4: Multiple TransitWithArgs calls
// ============================================================================
namespace multiple_transits_test {

FSM_EVENT(EvNext) {
  int step;
  explicit EvNext(int s) : step(s) {}
};

struct Step1;
struct Step2;
struct Step3;

FSM_STATE_MACHINE(StepMachine, Step1) {};

FSM_STATE(Step1, StepMachine) {
  int val_ = 0;
  Step1() = default;
  explicit Step1(int v) : val_(v) {}

  ufsm::Result React(const EvNext& e) {
    return Transit<Step2>(ufsm::with_args, val_ + e.step);
  }
  using reactions = ufsm::List<ufsm::Reaction<EvNext>>;
};

FSM_STATE(Step2, StepMachine) {
  int val_;
  explicit Step2(int v) : val_(v) {}

  ufsm::Result React(const EvNext& e) {
    return Transit<Step3>(ufsm::with_args, val_ + e.step);
  }
  using reactions = ufsm::List<ufsm::Reaction<EvNext>>;
};

FSM_STATE(Step3, StepMachine) {
  int val_;
  explicit Step3(int v) : val_(v) {}

  int GetValue() const { return val_; }
  using reactions = ufsm::List<>;
};

}  // namespace multiple_transits_test

TEST(TransitWithArgsTest, MultipleConsecutiveTransitsWithArgs) {
  using namespace multiple_transits_test;

  StepMachine m;
  m.Initiate();
  EXPECT_TRUE(m.IsInState<Step1>());

  m.ProcessEvent(EvNext{10});  // 0 + 10 = 10
  EXPECT_TRUE(m.IsInState<Step2>());

  m.ProcessEvent(EvNext{5});   // 10 + 5 = 15
  EXPECT_TRUE(m.IsInState<Step3>());
}

// ============================================================================
// Test 5: Transit with both action and args
// ============================================================================
namespace action_and_args_test {

std::vector<std::string> log;

FSM_EVENT(EvGo) {};

struct StateA;
struct StateB;

FSM_STATE_MACHINE(Machine, StateA) {};

FSM_STATE(StateA, Machine) {
  StateA() { log.push_back("StateA::ctor"); }
  ~StateA() { log.push_back("StateA::dtor"); }

  ufsm::Result React(const EvGo&) {
    // Transit with both action and constructor arguments
    return Transit<StateB>([](){ log.push_back("action_executed"); }, ufsm::with_args, 42, "hello");
  }

  using reactions = ufsm::List<ufsm::Reaction<EvGo>>;
};

FSM_STATE(StateB, Machine) {
  int value_;
  std::string msg_;

  StateB(int v, std::string m) : value_(v), msg_(std::move(m)) {
    log.push_back("StateB::ctor(" + std::to_string(value_) + "," + msg_ + ")");
  }
  ~StateB() { log.push_back("StateB::dtor"); }

  using reactions = ufsm::List<>;
};

}  // namespace action_and_args_test

TEST(TransitWithArgsTest, TransitWithBothActionAndArgs) {
  using namespace action_and_args_test;
  log.clear();

  Machine m;
  m.Initiate();
  EXPECT_EQ(log.back(), "StateA::ctor");

  m.ProcessEvent(EvGo{});

  EXPECT_TRUE(m.IsInState<StateB>());

  // Verify order: StateA dtor -> action -> StateB ctor
  auto it_dtor = std::find(log.begin(), log.end(), "StateA::dtor");
  auto it_action = std::find(log.begin(), log.end(), "action_executed");
  auto it_ctor = std::find(log.begin(), log.end(), "StateB::ctor(42,hello)");

  ASSERT_NE(it_dtor, log.end());
  ASSERT_NE(it_action, log.end());
  ASSERT_NE(it_ctor, log.end());

  EXPECT_LT(it_dtor, it_action);    // dtor before action
  EXPECT_LT(it_action, it_ctor);    // action before ctor
}

}  // anonymous namespace
