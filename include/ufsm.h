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

template <typename T>
struct ContextContainer {
  T *ctx_ptr_{};
};

template <typename T>
static void *Constructor() {
  return new T;
}

}  // namespace detail

class Transition {
 private:
  using CallType = void *();
  CallType &state_creator_;
  static void *NoTransit() { return nullptr; }

 public:
  explicit Transition(CallType &call = NoTransit) : state_creator_(call) {}
  void *operator()() { return state_creator_(); }
  explicit operator bool() const { return &state_creator_ != &NoTransit; }
};

class NoTransit : public Transition {};

template <typename StateBase, typename StateContext = int>
class State : public detail::ContextContainer<StateContext> {
 private:
  // Prevent external modification
  using detail::ContextContainer<StateContext>::ctx_ptr_;

 public:
  using BaseType = StateBase;
  using ContextType = StateContext;

 protected:
  // Only allow inheritance, no construction
  virtual ~State() = default;

  ContextType &Context() { return *ctx_ptr_; }

  template <typename NewState>
  Transition Transit() {
    static_assert(detail::IsSameType<typename NewState::BaseType, BaseType>{},
                  "");
    return Transition{detail::Constructor<NewState>};
  }
};

template <typename StateBase>
class Fsm {
 private:
  StateBase *state_{};
  typename StateBase::ContextType context_{};

  void Transit(void *next_state) {
    if (!next_state) return;
    state_ = reinterpret_cast<decltype(state_)>(next_state);
    // Set context
    detail::ContextContainer<typename StateBase::ContextType> &ctx = *state_;
    ctx.ctx_ptr_ = &context_;
  }

 public:
  template <typename InitState>
  void Initiate() {
    static_assert(detail::IsSameType<typename InitState::BaseType,
                                     typename StateBase::BaseType>{},
                  "");
    delete state_;
    Transit(detail::Constructor<InitState>());
  }

  const typename StateBase::ContextType &Context() const { return context_; }

  template <typename E>
  void ProcessEvent(const E &event) {
    if (!state_) return;
    if (Transition transition = state_->React(event)) {
      delete state_;
      Transit(transition());
    }
  }
  virtual ~Fsm() { delete state_; }
};

}  // namespace ufsm
