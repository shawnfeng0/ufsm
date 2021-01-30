#pragma once

namespace ufsm {

namespace detail {

template <typename T1, typename T2>
struct IsSameType {
  constexpr operator bool() { return false; }
};

template <typename T>
struct IsSameType<T, T> {
  constexpr operator bool() { return true; }
};

static constexpr void *NoTransit() { return nullptr; }

template <typename NewState>
static void *Constructor() {
  return new NewState;
}

}  // namespace detail

struct Event {};

class Transition {
  using CallType = void *();

 public:
  Transition(CallType &call = detail::NoTransit) : state_creator_(call) {}
  void *operator()() { return state_creator_(); }
  operator bool() const { return &state_creator_ != &detail::NoTransit; }

 private:
  CallType &state_creator_;
};

class NoTransit : public Transition {};

template <typename T>
class State {
  // Only allow inheritance, no construction
 protected:
  State() = default;
  virtual ~State() = default;

  template <typename NewState>
  Transition Transit() {
    static_assert(detail::IsSameType<typename NewState::FsmType, FsmType>{},
                  "");
    return Transition{detail::Constructor<NewState>};
  }

 public:
  using FsmType = T;
};

template <typename StateBase>
class Fsm {
  StateBase *state_{};

 public:
  template <typename InitState>
  void Initiate() {
    static_assert(detail::IsSameType<typename InitState::FsmType,
                                     typename StateBase::FsmType>{},
                  "");
    delete state_;
    state_ =
        reinterpret_cast<decltype(state_)>(detail::Constructor<InitState>());
  }

  template <typename E>
  void ProcessEvent(const E &event) {
    auto transition = state_->Process(event);
    if (transition) {
      delete state_;
      state_ = reinterpret_cast<decltype(state_)>(transition());
    }
  }
  virtual ~Fsm() { delete state_; }
};

}  // namespace ufsm
