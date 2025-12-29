#ifndef UFSM_UFSM_H_
#define UFSM_UFSM_H_

#include <cassert>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef UFSM_ASSERT
#if !defined(NDEBUG)
#define UFSM_ASSERT(expr) assert(expr)
#else
#define UFSM_ASSERT(expr) ((void)0)
#endif
#endif

namespace ufsm {

// Metaprogramming utilities
namespace mp {

// A type list container.
// Example: List<int, float, char>
template <typename... Types>
struct List {};

// Prepend a type to a list.
// Example: PushFront<List<B, C>, A>::type -> List<A, B, C>
template <typename ListType, typename Type>
struct PushFront;

template <typename... Types, typename Type>
struct PushFront<List<Types...>, Type> {
  using type = List<Type, Types...>;
};

// Append a type to a list.
// Example: PushBack<List<A, B>, C>::type -> List<A, B, C>
template <typename ListType, typename Type>
struct PushBack;

template <typename... Types, typename Type>
struct PushBack<List<Types...>, Type> {
  using type = List<Types..., Type>;
};

// Check if a type is in a list.
// Example: Contains<List<A, B>, A>::value -> true
template <typename ListType, typename Type>
struct Contains : std::false_type {};

template <typename... Types, typename Type>
struct Contains<List<Types...>, Type> : std::bool_constant<(std::is_same_v<Types, Type> || ...)> {};

// Find the first type in a list satisfying a predicate.
// Pred is a template class where Pred<T>::value is a boolean.
template <typename ListType, template <typename> class Pred>
struct FindIf {
  using type = void;
};

template <typename Head, typename... Tail, template <typename> class Pred>
struct FindIf<List<Head, Tail...>, Pred> {
  using type = std::conditional_t<Pred<Head>::value, Head, typename FindIf<List<Tail...>, Pred>::type>;
};

// Find the first type in List1 that is also present in List2.
template <typename List1, typename List2>
struct FindFirstCommon {
  template <typename T>
  using Pred = Contains<List2, T>;
  using type = typename FindIf<List1, Pred>::type;
};

// Find a child state in ListType whose context is ParentType.
template <typename ListType, typename ParentType>
struct FindChildOf {
  template <typename T>
  using Pred = std::is_same<typename T::ContextType, ParentType>;
  using type = typename FindIf<ListType, Pred>::type;
};

// Build a path of states from the head of ListType up to (but not including) StopType.
// Used to determine the sequence of states to construct during a transition.
template <typename ListType, typename StopType>
struct BuildPath {
  using type = List<>;
};

template <typename Head, typename... Tail, typename StopType>
struct BuildPath<List<Head, Tail...>, StopType> {
  using type = std::conditional_t<std::is_same_v<Head, StopType>, List<>,
                                  typename PushBack<typename BuildPath<List<Tail...>, StopType>::type, Head>::type>;
};

}  // namespace mp

// Alias for creating a type list.
template <typename... Types>
using List = mp::List<Types...>;

// Result of an event reaction.
enum class Result {
  kNoReaction,    // Event was not handled by this reaction.
  kForwardEvent,  // Event was not handled by the state, forward to parent.
  kDiscardEvent,  // Event was handled (discarded) by the state, stop processing.
  kDeferEvent,    // Event was deferred by the state.
  kConsumed       // Event was consumed by the state.
};

// Tag type for passing constructor arguments in Transit.
// Usage: Transit<DestState>(ufsm::with_args, arg1, arg2, ...)
struct with_args_t { explicit with_args_t() = default; };
inline constexpr with_args_t with_args{};

// Forward declarations
template <class EventType>
class Reaction;

template <class EventType, class DestState, class Action>
class Transition;

template <class EventType>
class Deferral;

template <typename Derived, typename InnerInitial>
class StateMachine;

template <typename Derived, typename ContextState, typename InnerInitial>
class State;

namespace detail {

// Helper to get a pretty type name for debugging.
// Extracts the type name from the compiler-specific function signature.
template <typename T>
constexpr std::string_view PrettyTypeName() {
#if defined(__clang__)
  std::string_view p = __PRETTY_FUNCTION__, s = "T = ";
  auto start = p.find(s), end = p.rfind(']');
  return (start != std::string_view::npos && end > start + 4) ? p.substr(start + 4, end - start - 4) : p;
#elif defined(__GNUC__)
  std::string_view p = __PRETTY_FUNCTION__, s = "T = ";
  auto start = p.find(s), end = p.find(';', start);
  return (start != std::string_view::npos && end > start + 4) ? p.substr(start + 4, end - start - 4) : p;
#elif defined(_MSC_VER)
  std::string_view p = __FUNCSIG__, s = "PrettyTypeName<";
  auto start = p.find(s);
  if (start == std::string_view::npos) return p;
  auto inner = p.substr(start + s.size()), end = inner.find(">(");
  return end != std::string_view::npos ? inner.substr(0, end) : p;
#else
  return "<type>";
#endif
}

// RAII helper to restore a variable's value on scope exit.
template <class T>
struct RestoreOnExit {
  T& slot;
  T prev;
  RestoreOnExit(T& s, T v) : slot(s), prev(std::exchange(s, v)) {}
  ~RestoreOnExit() { slot = prev; }
};

// Base class for all events.
class EventBase {
 public:
  using Ptr = std::unique_ptr<EventBase>;
  virtual ~EventBase() = default;
  const void* TypeId() const noexcept { return type_id_; }
  virtual const char* Name() const noexcept { return "<ufsm::event>"; }
  virtual Ptr Clone() const = 0;

 protected:
  constexpr explicit EventBase(const void* type_id) noexcept : type_id_(type_id) {}

 private:
  const void* type_id_;
};

// Base class for all states.
class StateBase {
 public:
  virtual const char* Name() const noexcept { return "<ufsm::state>"; }
  virtual void Exit() {}
  virtual ~StateBase() = default;

 protected:
  StateBase() = default;

  virtual Result ReactImpl(const EventBase& event) = 0;
  virtual const void* TypeId() const noexcept = 0;

 private:
  std::size_t ActiveIndex() const noexcept { return active_index_; }
  void SetActiveIndex(std::size_t index) noexcept { active_index_ = index; }

  template <typename, typename, typename>
  friend class ::ufsm::State;
  template <typename, typename>
  friend class ::ufsm::StateMachine;

  std::size_t active_index_ = 0;
};

// SFINAE check for OnUnhandledEvent method.
template <typename MachineType, typename = void>
struct HasOnUnhandledEventMethod : std::false_type {};

template <typename MachineType>
struct HasOnUnhandledEventMethod<
    MachineType, std::void_t<decltype(std::declval<MachineType&>().OnUnhandledEvent(std::declval<const EventBase&>()))>>
    : std::true_type {};

template <typename MachineType>
inline void InvokeOnUnhandledEventIfPresent(MachineType& machine, const EventBase& event) {
  if constexpr (HasOnUnhandledEventMethod<MachineType>::value) machine.OnUnhandledEvent(event);
}

// SFINAE check for OnEventProcessed method.
template <typename MachineType, typename = void>
struct HasOnEventProcessedMethod : std::false_type {};

template <typename MachineType>
struct HasOnEventProcessedMethod<
    MachineType, std::void_t<decltype(std::declval<MachineType&>().OnEventProcessed(
                     std::declval<const StateBase*>(), std::declval<const EventBase&>(), std::declval<Result>()))>>
    : std::true_type {};

template <typename MachineType>
inline void InvokeOnEventProcessedIfPresent(MachineType& machine, const StateBase* leaf_state, const EventBase& event,
                                            Result result) {
  if constexpr (HasOnEventProcessedMethod<MachineType>::value) machine.OnEventProcessed(leaf_state, event, result);
}

// SFINAE check for OnEntry method.
template <typename StateType, typename = void>
struct HasOnEntryMethod : std::false_type {};

template <typename StateType>
struct HasOnEntryMethod<StateType, std::void_t<decltype(std::declval<StateType&>().OnEntry())>> : std::true_type {};

// SFINAE check for OnExit method.
template <typename StateType, typename = void>
struct HasOnExitMethod : std::false_type {};

template <typename StateType>
struct HasOnExitMethod<StateType, std::void_t<decltype(std::declval<StateType&>().OnExit())>> : std::true_type {};

// Helper to construct a chain of states.
// Recursively constructs states from Head to Tail.
template <typename ContextList, typename OutermostContext>
struct Constructor;

template <typename Head, typename... Tail, typename OutermostContext>
struct Constructor<mp::List<Head, Tail...>, OutermostContext> {
  template <typename ContextPtr, typename Factory = std::nullptr_t>
  static void Construct(const ContextPtr& context_ptr, OutermostContext& state_machine, Factory&& factory = nullptr) {
    if constexpr (sizeof...(Tail) == 0)
      Head::DeepConstruct(context_ptr, state_machine, std::forward<Factory>(factory));  // Pass factory only to final state
    else
      Constructor<mp::List<Tail...>, OutermostContext>::Construct(Head::ShallowConstruct(context_ptr, state_machine), state_machine, std::forward<Factory>(factory));
  }
};

// Helper to build the context list for a transition.
// Constructs the list of states that need to be entered to reach DestState from CurrentContext.
template <class CurrentContext, class DestState>
struct MakeContextList {
  using type = typename mp::PushBack<typename mp::BuildPath<typename DestState::ContextTypeList, CurrentContext>::type,
                                     DestState>::type;
};

}  // namespace detail

// CRTP base class for events.
template <typename Derived>
class Event : public detail::EventBase {
 public:
  Event() noexcept : detail::EventBase(StaticTypeId()) {
    static_assert(std::is_base_of_v<Event, Derived>, "Derived event must inherit from ufsm::Event<Derived>");
  }
  const char* Name() const noexcept override {
    static const std::string name{detail::PrettyTypeName<Derived>()};
    return name.c_str();
  }
  detail::EventBase::Ptr Clone() const override {
    return detail::EventBase::Ptr(new Derived(static_cast<const Derived&>(*this)));
  }

 private:
  template <class>
  friend class ::ufsm::Reaction;
  template <class, class, class>
  friend class ::ufsm::Transition;
  template <class>
  friend class ::ufsm::Deferral;
  static const void* StaticTypeId() noexcept {
    static const int tag = 0;
    return &tag;
  }
};

// Reaction to a specific event type.
template <class EventType>
class Reaction {
  static_assert(std::is_base_of_v<detail::EventBase, EventType>, "EventType must be derived from ufsm::Event");

 public:
  template <typename StateType, typename EventBaseType>
  static Result React(StateType& state, const EventBaseType& event) {
    if (event.TypeId() == EventType::StaticTypeId()) {
      using ReturnT = decltype(state.React(static_cast<const EventType&>(event)));
      if constexpr (std::is_void_v<ReturnT>) {
        state.React(static_cast<const EventType&>(event));
        return Result::kConsumed;
      } else
        return state.React(static_cast<const EventType&>(event));
    }
    return Result::kNoReaction;
  }
};

// Transition to a destination state upon a specific event.
template <class EventType, class DestState, class Action = std::nullptr_t>
class Transition {
  static_assert(std::is_base_of_v<detail::EventBase, EventType>, "EventType must be derived from ufsm::Event");
  static_assert(std::is_base_of_v<detail::StateBase, DestState>, "DestState must be derived from ufsm::State");

 public:
  template <typename StateType, typename EventBaseType>
  static Result React(StateType& state, const EventBaseType& event) {
    if (event.TypeId() == EventType::StaticTypeId()) {
      if constexpr (std::is_same_v<Action, std::nullptr_t>)
        return state.template Transit<DestState>();
      else
        return state.template Transit<DestState>(Action{});
    }
    return Result::kNoReaction;
  }
};

// Deferral of a specific event type.
template <class EventType>
class Deferral {
  static_assert(std::is_base_of_v<detail::EventBase, EventType> || std::is_same_v<EventType, detail::EventBase>,
                "EventType must be derived from ufsm::Event");

 public:
  template <typename StateType, typename EventBaseType>
  static Result React(StateType& state, const EventBaseType& event) {
    if constexpr (std::is_same_v<EventType, detail::EventBase>) {
      return state.DeferEvent();
    } else {
      if (event.TypeId() == EventType::StaticTypeId()) return state.DeferEvent();
    }
    return Result::kNoReaction;
  }
};

// CRTP base class for states.
// Derived: The concrete state class.
// ContextState: The parent state or state machine.
// InnerInitial: The initial substate (if any).
template <typename Derived, typename ContextState, typename InnerInitial = void>
class State : public detail::StateBase {
  static_assert(std::is_class_v<ContextState>, "ContextState must be a class (State or StateMachine)");

 public:
  using InnerInitialType = InnerInitial;
  using reactions = mp::List<>;
  using ContextType = typename ContextState::InnerContextType;
  using ContextPtrType = typename ContextType::InnerContextPtrType;
  using OutermostContextType = typename ContextType::OutermostContextType;
  using InnerContextType = Derived;
  using InnerContextPtrType = Derived*;
  using OutermostContextBaseType = typename ContextType::OutermostContextBaseType;
  using ContextTypeList = typename mp::PushFront<typename ContextType::ContextTypeList, ContextType>::type;

  // Access the outermost context (the state machine).
  const OutermostContextType& OutermostContext() const { return context_->OutermostContext(); }
  OutermostContextType& OutermostContext() {
    return const_cast<OutermostContextType&>(std::as_const(*this).OutermostContext());
  }

  // Post an event to the queue.
  template <class Ev>
  void PostEvent(Ev&& event) {
    OutermostContext().PostEvent(std::forward<Ev>(event));
  }

  // Defer the current event.
  [[nodiscard]] Result DeferEvent() {
    UFSM_ASSERT(context_->OutermostContextBase().current_event_ != nullptr);
    deferred_flag_ = true;
    return Result::kDeferEvent;
  }

  // Access a specific context in the hierarchy.
  template <class TargetContext>
  const TargetContext& Context() const {
    if constexpr (std::is_same_v<TargetContext, Derived> || std::is_base_of_v<TargetContext, Derived>) {
      return *static_cast<const Derived*>(this);
    } else {
      return context_->template Context<TargetContext>();
    }
  }

  template <class TargetContext>
  TargetContext& Context() {
    return const_cast<TargetContext&>(std::as_const(*this).template Context<TargetContext>());
  }

  // Initiate a transition to DestState.
  // Usage:
  //   Transit<DestState>()                            - basic transition
  //   Transit<DestState>(action)                      - with transition action
  //   Transit<DestState>(with_args, args...)          - with constructor arguments
  //   Transit<DestState>(action, with_args, args...)  - with both
  template <typename DestState, typename Action = std::nullptr_t>
  [[nodiscard]] Result Transit(Action&& action = nullptr) {
    return TransitImpl<DestState>(std::forward<Action>(action));
  }

  template <typename DestState, typename... Args>
  [[nodiscard]] Result Transit(with_args_t, Args&&... args) {
    return Transit<DestState>(nullptr, with_args, std::forward<Args>(args)...);
  }

  template <typename DestState, typename Action, typename... Args>
  [[nodiscard]] Result Transit(Action&& action, with_args_t, Args&&... args) {
    auto factory = [t = std::make_tuple(std::forward<Args>(args)...)](auto ctx) mutable {
      return std::apply([ctx](auto&&... a) {
        if constexpr (std::is_constructible_v<DestState, decltype(ctx), decltype(a)...>)
          return std::make_unique<DestState>(ctx, std::move(a)...);
        else {
          auto state = std::make_unique<DestState>(std::move(a)...);
          state->context_ = ctx;
          return state;
        }
      }, std::move(t));
    };
    return TransitImpl<DestState>(std::forward<Action>(action), std::move(factory));
  }

  // Helper methods for reaction results.
  [[nodiscard]] constexpr Result ForwardEvent() const noexcept { return Result::kForwardEvent; }
  [[nodiscard]] constexpr Result DiscardEvent() const noexcept { return Result::kDiscardEvent; }
  [[nodiscard]] constexpr Result ConsumeEvent() const noexcept { return Result::kConsumed; }

  const void* TypeId() const noexcept override { return StaticTypeId(); }
  const char* Name() const noexcept override {
    static const std::string name{detail::PrettyTypeName<Derived>()};
    return name.c_str();
  }

  void Exit() override {
    if constexpr (detail::HasOnExitMethod<Derived>::value) {
      static_cast<Derived*>(this)->OnExit();
    }
  }

 protected:
  State(ContextPtrType context = nullptr) : context_(context) {}
  ~State() {
    // If events were deferred, move them to the posted events queue upon state destruction.
    if (context_ && deferred_flag_) {
      auto& out = context_->OutermostContextBase();
      for (; !out.deferred_events_.empty(); out.deferred_events_.pop_back())
        out.posted_events_.push_front(std::move(out.deferred_events_.back()));
    }
  }

 private:
  static const void* StaticTypeId() noexcept {
    static const int tag = 0;
    return &tag;
  }

  const OutermostContextBaseType& OutermostContextBase() const { return context_->OutermostContextBase(); }
  OutermostContextBaseType& OutermostContextBase() { return context_->OutermostContextBase(); }

  template <typename DestState, typename Action, typename Factory = std::nullptr_t>
  [[nodiscard]] Result TransitImpl(Action&& action, Factory&& factory = nullptr) {
    // Find the common ancestor (Least Common Ancestor) between the current state and the destination state.
    // We include Derived (current state) in the search list to handle drill-down transitions correctly.
    using CommonAncestorType = typename mp::FindFirstCommon<typename mp::PushFront<ContextTypeList, Derived>::type,
                                                            typename DestState::ContextTypeList>::type;

    auto& state_machine = OutermostContextBase();

#if !defined(NDEBUG)
    UFSM_ASSERT(!state_machine.in_transition_);
    struct UfsmTransitionGuard {
      bool& flag;
      ~UfsmTransitionGuard() { flag = false; }
    } guard{state_machine.in_transition_ = true};
#endif

    // If the common ancestor is the current state class itself (Derived),
    // it means we are transitioning to a child state (drilling down).
    // Note: Self-transitions (DestState == Derived) are NOT handled here because
    // Derived is not in DestState::ContextTypeList, so LCA would be Parent.
    if constexpr (std::is_same_v<CommonAncestorType, Derived>) {
      // We are drilling down. We keep the current state active
      // and terminate anything below it (if any).
      state_machine.TerminateAfter(*this);

      // Execute transition action.
      // For drill-down, the action is executed on 'this' (Derived).
      if constexpr (!std::is_same_v<std::remove_reference_t<Action>, std::nullptr_t>) {
        if constexpr (std::is_invocable_v<Action&&, Derived&>)
          std::forward<Action>(action)(*static_cast<Derived*>(this));
        else
          std::forward<Action>(action)();
      }

      // Construct the new state path from 'this' to DestState.
      detail::Constructor<typename detail::MakeContextList<Derived, DestState>::type,
                          OutermostContextBaseType>::Construct(static_cast<Derived*>(this), state_machine, std::forward<Factory>(factory));

    } else {
      // Standard case: LCA is an ancestor.
      // Find the child of the common ancestor that is on the path to the current state.
      // We need to terminate from this state downwards.
      using StateToTerminateFromType =
          typename mp::FindChildOf<typename mp::PushFront<ContextTypeList, Derived>::type, CommonAncestorType>::type;

      auto& state_to_terminate_from = Context<StateToTerminateFromType>();
      auto& common_ancestor = state_to_terminate_from.template Context<CommonAncestorType>();

      // Terminate states down to the common ancestor.
      state_machine.TerminateImpl(state_to_terminate_from);

      // Execute transition action.
      if constexpr (!std::is_same_v<std::remove_reference_t<Action>, std::nullptr_t>) {
        if constexpr (std::is_invocable_v<Action&&, CommonAncestorType&>)
          std::forward<Action>(action)(common_ancestor);
        else
          std::forward<Action>(action)();
      }

      // Construct the new state path from the common ancestor to the destination state.
      detail::Constructor<typename detail::MakeContextList<CommonAncestorType, DestState>::type,
                          OutermostContextBaseType>::Construct(&common_ancestor, state_machine, std::forward<Factory>(factory));
    }

    return Result::kConsumed;
  }

  template <typename... Reactions>
  Result LocalReact(const detail::EventBase& event, mp::List<Reactions...>) {
    Result result = Result::kNoReaction;
    // Try each reaction in the list.
    // The fold expression uses the short-circuiting '||' operator.
    // If a reaction returns something other than kNoReaction, the expression becomes true,
    // and subsequent reactions are skipped.
    (void)((result = Reactions::React(*static_cast<Derived*>(this), event),
            result != Result::kNoReaction) ||
           ...);
    return result == Result::kNoReaction ? Result::kForwardEvent : result;
  }

  Result ReactImpl(const detail::EventBase& event) override {
    auto res = LocalReact(event, typename Derived::reactions{});
    // If not handled locally, forward to parent context.
    return res == Result::kForwardEvent ? context_->ReactImpl(event) : res;
  }

  template <typename, typename, typename>
  friend class ufsm::State;
  template <class, class>
  friend class ufsm::StateMachine;
  template <class, class>
  friend struct ufsm::detail::Constructor;

  template <typename Factory = std::nullptr_t>
  static void DeepConstruct(const ContextPtrType& context, OutermostContextBaseType& out_context, Factory&& factory = nullptr) {
    static_assert(std::is_base_of_v<State, Derived>, "Derived state must inherit from ufsm::State<Derived, ...>");
    static_assert(std::is_void_v<InnerInitial> || std::is_base_of_v<detail::StateBase, InnerInitial>,
                  "InnerInitial must be a State or void");
    auto* p_inner = ShallowConstruct(context, out_context, std::forward<Factory>(factory));
    if constexpr (!std::is_same_v<void, InnerInitial>) InnerInitial::DeepConstruct(p_inner, out_context);
  }

  template <typename Factory = std::nullptr_t>
  static InnerContextPtrType ShallowConstruct(const ContextPtrType& context, OutermostContextBaseType& out_context, [[maybe_unused]] Factory&& factory = nullptr) {
    std::unique_ptr<Derived> state;
    if constexpr (!std::is_same_v<std::decay_t<Factory>, std::nullptr_t>) {
      state = factory(context);  // TransitWithArgs path
    } else if constexpr (std::is_constructible_v<Derived, ContextPtrType>) {
      state = std::make_unique<Derived>(context);
    } else if constexpr (std::is_default_constructible_v<Derived>) {
      state = std::make_unique<Derived>();
      state->context_ = context;
    } else {
      static_assert(!sizeof(Derived), "State requires constructor arguments. Use TransitWithArgs().");
    }
    auto* ptr = out_context.Add(std::move(state));
    if constexpr (detail::HasOnEntryMethod<Derived>::value) ptr->OnEntry();
    return ptr;
  }

  ContextPtrType context_;
  bool deferred_flag_ = false;
};

// The root of the state hierarchy.
template <typename Derived, typename InnerInitial>
class StateMachine {
 public:
  using InnerContextType = Derived;
  using OutermostContextType = Derived;
  using OutermostContextBaseType = StateMachine;
  using InnerContextPtrType = Derived*;
  using ContextTypeList = mp::List<>;
  using ContextType = void;

  // Start the state machine.
  void Initiate() {
    static_assert(std::is_base_of_v<StateMachine, Derived>,
                  "Derived machine must inherit from ufsm::StateMachine<Derived, ...>");
    static_assert(std::is_base_of_v<detail::StateBase, InnerInitial>, "InnerInitial must be a State");
    TerminateImpl();
    InnerInitial::DeepConstruct(static_cast<Derived*>(this), *static_cast<Derived*>(this));
  }

  // Stop the state machine.
  void Terminate() {
#if !defined(NDEBUG)
    UFSM_ASSERT(!in_transition_);
#endif
    TerminateImpl();
  }

  bool Terminated() const noexcept { return !current_state_; }

  // Process an event.
  // This method handles the event loop, ensuring that posted events are processed
  // after the current event is handled. It also handles re-entrant calls.
  template <class Ev>
  Result ProcessEvent(const Ev& event) {
    return ProcessEventLoop(event);
  }

  // Post an event to be processed later.
  template <class Ev>
  void PostEvent(Ev&& event) {
    posted_events_.push_back(static_cast<const detail::EventBase&>(event).Clone());
  }

  // Check if the machine is in a specific state.
  template <class StateT>
  bool IsInState() const {
    static_assert(std::is_base_of_v<detail::StateBase, StateT>, "StateT must be a state");
    for (const auto& p : active_path_)
      if (p && p->TypeId() == StateT::StaticTypeId()) return true;
    return false;
  }

  template <class TargetContext = Derived>
  const TargetContext& Context() const {
    return *static_cast<const Derived*>(this);
  }
  template <class TargetContext = Derived>
  TargetContext& Context() {
    return *static_cast<Derived*>(this);
  }

  const Derived& OutermostContext() const { return *static_cast<const Derived*>(this); }
  Derived& OutermostContext() { return *static_cast<Derived*>(this); }

 protected:
  StateMachine() { active_path_.reserve(8); }
  virtual ~StateMachine() { TerminateImpl(); }

 private:
  const StateMachine& OutermostContextBase() const { return *this; }
  StateMachine& OutermostContextBase() { return *this; }

  template <typename, typename, typename>
  friend class ufsm::State;

#if !defined(NDEBUG)
  bool in_transition_ = false;
#endif

  Result ReactImpl(const detail::EventBase&) { return Result::kForwardEvent; }

  template <class StateType>
  StateType* Add(std::unique_ptr<StateType> state_ptr) {
    state_ptr->SetActiveIndex(active_path_.size());
    StateType* raw_ptr = state_ptr.get();
    active_path_.push_back(std::move(state_ptr));
    if constexpr (std::is_same_v<typename StateType::InnerInitialType, void>) current_state_ = raw_ptr;
    return raw_ptr;
  }

  Result ProcessEventLoop(const detail::EventBase& event) {
#if !defined(NDEBUG)
    UFSM_ASSERT(!in_transition_);
#endif
    // If we are already in the event loop (re-entrant call), just process the event immediately.
    if (in_event_loop_) return ProcessEventImpl(event);

    // Otherwise, mark that we are in the event loop.
    detail::RestoreOnExit<bool> guard(in_event_loop_, true);
    Result last = ProcessEventImpl(event);

    // Process posted events after the current event.
    // This queue may grow as events are processed.
    for (; !posted_events_.empty(); posted_events_.pop_front()) last = ProcessEventImpl(*posted_events_.front());
    return last;
  }

  Result ProcessEventImpl(const detail::EventBase& event) {
    detail::RestoreOnExit<const detail::EventBase*> guard(current_event_, &event);

    // Delegate reaction to the current state.
    auto res = current_state_ ? current_state_->ReactImpl(event) : Result::kForwardEvent;

    detail::InvokeOnEventProcessedIfPresent(*static_cast<Derived*>(this), current_state_, event, res);

    // Handle deferral.
    if (res == Result::kDeferEvent) {
      deferred_events_.push_back(event.Clone());
      return Result::kConsumed;
    }

    // Handle unhandled events.
    if (res == Result::kForwardEvent) detail::InvokeOnUnhandledEventIfPresent(*static_cast<Derived*>(this), event);
    return res;
  }

  void ResetToDepth(std::size_t n) {
    while (active_path_.size() > n) {
      active_path_.back()->Exit();
      active_path_.pop_back();
    }
    current_state_ = active_path_.empty() ? nullptr : active_path_.back().get();
  }

  void TerminateImpl() {
    ResetToDepth(0);
    posted_events_.clear();
    deferred_events_.clear();
  }

  void TerminateImpl(detail::StateBase& state) {
    if (active_path_.empty()) {
      current_state_ = nullptr;
      return;
    }
    auto idx = state.ActiveIndex();
    UFSM_ASSERT(idx < active_path_.size() && active_path_[idx].get() == &state);
    // Reset the state machine to the depth of the state being terminated.
    // This effectively exits all states deeper than 'state'.
    ResetToDepth(idx < active_path_.size() && active_path_[idx].get() == &state ? idx : 0);
  }

  void TerminateAfter(detail::StateBase& state) {
    if (active_path_.empty()) {
      current_state_ = nullptr;
      return;
    }
    auto idx = state.ActiveIndex();
    UFSM_ASSERT(idx < active_path_.size() && active_path_[idx].get() == &state);
    // Reset the state machine to the depth after the state.
    // This effectively exits all states deeper than 'state', but keeps 'state' active.
    ResetToDepth(idx < active_path_.size() && active_path_[idx].get() == &state ? idx + 1 : 0);
  }

  detail::StateBase* current_state_ = nullptr;
  std::vector<std::unique_ptr<detail::StateBase>> active_path_;
  const detail::EventBase* current_event_ = nullptr;
  std::vector<detail::EventBase::Ptr> deferred_events_;
  std::deque<detail::EventBase::Ptr> posted_events_;
  bool in_event_loop_ = false;
};

}  // namespace ufsm

// Macros for defining state machines, states, and events.
#define FSM_STATE_MACHINE(machine_type, initial_state_type) \
  struct initial_state_type;                                \
  struct machine_type : ufsm::StateMachine<machine_type, initial_state_type>
#define _FSM_STATE_2(state_type, parent_state_type) \
  struct parent_state_type;                         \
  struct state_type : ufsm::State<state_type, parent_state_type>
#define _FSM_STATE_3(state_type, parent_state_type, initial_state_type) \
  struct parent_state_type;                                             \
  struct initial_state_type;                                            \
  struct state_type : ufsm::State<state_type, parent_state_type, initial_state_type>
#define FSM_STATE(...) _FSM_GET_MACRO(__VA_ARGS__, _FSM_STATE_3, _FSM_STATE_2)(__VA_ARGS__)
#define _FSM_GET_MACRO(first_arg, second_arg, third_arg, selected_macro, ...) selected_macro
#define FSM_EVENT(event_type) struct event_type : ufsm::Event<event_type>

#endif  // UFSM_UFSM_H_
