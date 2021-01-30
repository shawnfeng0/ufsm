#pragma once

namespace ufsm {

namespace detail {

template <typename T1, typename T2>
struct IsSameType {
  constexpr explicit operator bool() { return false; }
};

template <typename T>
struct IsSameType<T, T> {
  constexpr explicit operator bool() { return true; }
};

static constexpr void *NoTransit() { return nullptr; }

template <typename NewState>
static void *Constructor() {
  return new NewState;
}

struct ContextContainer {
  void *data_ptr_{};
};

}  // namespace detail

struct Event {};

class Transition {
 private:
  using CallType = void *();
  CallType &state_creator_;

 public:
  explicit Transition(CallType &call = detail::NoTransit)
      : state_creator_(call) {}
  void *operator()() { return state_creator_(); }
  explicit operator bool() const {
    return &state_creator_ != &detail::NoTransit;
  }
};

class NoTransit : public Transition {};

template <typename StateBase, typename StateContext>
class State : public detail::ContextContainer {
 private:
  // Prevent external modification
  using detail::ContextContainer::data_ptr_;

 public:
  using FsmType = StateBase;
  using ContextType = StateContext;

 protected:
  // Only allow inheritance, no construction
  virtual ~State() = default;

  ContextType &Context() { return *reinterpret_cast<ContextType *>(data_ptr_); }

  template <typename NewState>
  Transition Transit() {
    static_assert(detail::IsSameType<typename NewState::FsmType, FsmType>{},
                  "");
    return Transition{detail::Constructor<NewState>};
  }
};

template <typename StateBase>
class Fsm {
  StateBase *state_{};
  typename StateBase::ContextType *context_{};

 private:
  void Transit(void *next_state) {
    if (!next_state) return;
    state_ = reinterpret_cast<decltype(state_)>(next_state);
    // Set context
    detail::ContextContainer &ctx = *state_;
    ctx.data_ptr_ = context_;
  }

 public:
  Fsm() { context_ = new typename StateBase::ContextType; }
  template <typename InitState>
  void Initiate() {
    static_assert(detail::IsSameType<typename InitState::FsmType,
                                     typename StateBase::FsmType>{},
                  "");
    delete state_;
    Transit(detail::Constructor<InitState>());
  }

  const typename StateBase::ContextType &Context() const { return *context_; }

  template <typename E>
  void ProcessEvent(const E &event) {
    if (!state_) return;
    auto transition = state_->React(event);
    if (transition) {
      delete state_;
      Transit(transition());
    }
  }
  virtual ~Fsm() {
    delete state_;
    delete context_;
  }
};

}  // namespace ufsm
