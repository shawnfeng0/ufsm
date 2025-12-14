#pragma once

#include <cstddef>
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <deque>
#include <vector>

#ifndef UFSM_ASSERT
#if !defined(NDEBUG)
#define UFSM_ASSERT(expr) assert(expr)
#else
#define UFSM_ASSERT(expr) ((void)0)
#endif
#endif

namespace ufsm {

namespace mp {

template <typename... Types>
struct List {};

template <typename ListType, typename Type>
struct PushFront;

template <typename... Types, typename Type>
struct PushFront<List<Types...>, Type> {
    using type = List<Type, Types...>;
};

template <typename ListType, typename Type>
struct PushBack;

template <typename... Types, typename Type>
struct PushBack<List<Types...>, Type> {
    using type = List<Types..., Type>;
};

template <typename ListType, typename Type>
struct Contains;

template <typename... Types, typename Type>
struct Contains<List<Types...>, Type> : std::bool_constant<(std::is_same_v<Types, Type> || ...)> {};

template <typename List1, typename List2>
struct FindFirstCommon;

template <typename List2>
struct FindFirstCommon<List<>, List2> {
    using type = void;
};

template <typename Head, typename... Tail, typename List2>
struct FindFirstCommon<List<Head, Tail...>, List2> {
    using type = std::conditional_t<Contains<List2, Head>::value, Head, typename FindFirstCommon<List<Tail...>, List2>::type>;
};

template <typename ListType, typename ParentType>
struct FindChildOf;

template <typename ParentType>
struct FindChildOf<List<>, ParentType> {
    using type = void;
};

template <typename Head, typename... Tail, typename ParentType>
struct FindChildOf<List<Head, Tail...>, ParentType> {
    using type = std::conditional_t<std::is_same_v<typename Head::ContextType, ParentType>, Head, typename FindChildOf<List<Tail...>, ParentType>::type>;
};

template <typename ListType, typename StopType>
struct BuildPath;

template <typename StopType>
struct BuildPath<List<>, StopType> {
    using type = List<>;
};

template <typename Head, typename... Tail, typename StopType>
struct BuildPath<List<Head, Tail...>, StopType> {
    using type = std::conditional_t<std::is_same_v<Head, StopType>, List<>, typename PushBack<typename BuildPath<List<Tail...>, StopType>::type, Head>::type>;
};

} // namespace mp

// User-facing alias to avoid exposing the internal `mp` namespace in common code.
// Example:
//   using reactions = ufsm::List<ufsm::Reaction<Ev1>, ufsm::Transition<Ev2, Next>>;
template <typename... Types>
using List = mp::List<Types...>;
// Forward declaration for diagnostics traits (defined later).
// Event handling result types.
enum class Result {
    kNoReaction,   // Event not handled
    kForwardEvent, // Forward event to parent state
    kDiscardEvent, // Discard event (actively ignored)
    kDeferEvent,   // Defer the currently handled event
    kConsumed      // Event consumed (handled)
};

// Forward declaration for diagnostics traits (defined later).
template <class EventType>
class Reaction;

// Forward declarations for friend relationships between framework internals.
template <typename Derived, typename InnerInitial>
class StateMachine;

namespace detail {

// Best-effort, header-only type name extraction for diagnostics.
// This is intended for logging/tracing (e.g., OnUnhandledEvent), not for stable IDs.
template <typename T>
constexpr std::string_view PrettyTypeName() {
#if defined(__clang__)
    std::string_view p = __PRETTY_FUNCTION__;
    // "std::string_view ufsm::detail::PrettyTypeName() [T = X]"
    const auto start = p.find("T = ");
    const auto end = p.rfind(']');
    if (start == std::string_view::npos || end == std::string_view::npos || end <= start + 4) return p;
    return p.substr(start + 4, end - (start + 4));
#elif defined(__GNUC__)
    std::string_view p = __PRETTY_FUNCTION__;
    // "constexpr std::string_view ufsm::detail::PrettyTypeName() [with T = X; std::string_view = ...]"
    const auto start = p.find("T = ");
    if (start == std::string_view::npos) return p;
    const auto end = p.find(';', start);
    if (end == std::string_view::npos || end <= start + 4) return p;
    return p.substr(start + 4, end - (start + 4));
#elif defined(_MSC_VER)
    std::string_view p = __FUNCSIG__;
    // "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl ufsm::detail::PrettyTypeName<...>(void)"
    const auto start = p.find("PrettyTypeName<");
    if (start == std::string_view::npos) return p;
    const auto inner = p.substr(start + std::string_view("PrettyTypeName<").size());
    const auto end = inner.find(">(");
    if (end == std::string_view::npos) return p;
    return inner.substr(0, end);
#else
    return std::string_view{"<type>"};
#endif
}

template <typename T>
inline const char* PrettyTypeNameCStr() {
    static const std::string s{PrettyTypeName<T>()};
    return s.c_str();
}

template <typename T>
struct IsMpList : std::false_type {};

template <typename... Ts>
struct IsMpList<mp::List<Ts...>> : std::true_type {};

template <typename T>
struct IsReaction : std::false_type {};

template <typename EventType>
struct IsReaction<Reaction<EventType>> : std::true_type {};

template <typename T>
struct IsTransition : std::false_type {};

template <typename T>
struct IsDeferral : std::false_type {};

template <typename ListType>
struct AllAreReactionEntries;

template <typename... Ts>
struct AllAreReactionEntries<mp::List<Ts...>>
    : std::bool_constant<((IsReaction<Ts>::value || IsTransition<Ts>::value || IsDeferral<Ts>::value) && ...)> {};

template <typename StateType, typename EventType, typename = void>
struct HasReactMethod : std::false_type {};

template <typename StateType, typename EventType>
struct HasReactMethod<StateType,
                      EventType,
                      std::void_t<decltype(std::declval<StateType&>().React(std::declval<const EventType&>()))>>
    : std::true_type {};

template <typename StateType, typename EventType, typename = void>
struct ReactReturnType {
    using type = void;
};

template <typename StateType, typename EventType>
struct ReactReturnType<StateType,
                       EventType,
                       std::void_t<decltype(std::declval<StateType&>().React(std::declval<const EventType&>()))>> {
    using type = decltype(std::declval<StateType&>().React(std::declval<const EventType&>()));
};

class EventBase {
public:
    virtual ~EventBase() = default;

    using Ptr = std::unique_ptr<EventBase>;

    // Type identity for event dispatch.
    // NOTE: This is unique per process image, but not stable across DSOs/plugins.
    const void* TypeId() const noexcept { return type_id_; }

    // Human-readable name for diagnostics/logging.
    // Default implementation is generic; ufsm::Event<Derived> overrides this.
    virtual const char* Name() const noexcept { return "<ufsm::event>"; }

    // Polymorphic copy used by Boost.Statechart-like deferral of event_base.
    // Implemented by ufsm::Event<Derived>.
    virtual Ptr Clone() const = 0;

protected:
    constexpr explicit EventBase(const void* type_id) noexcept : type_id_(type_id) {}

private:
    const void* type_id_;
};

// Base class for all states, providing common interface.
// Ownership is handled by the outermost state machine (see StateMachine).
class StateBase {
public:
    virtual Result ReactImpl(const EventBase& event) = 0;

    // RTTI-free identity and name for diagnostics and state queries.
    virtual const void* TypeId() const noexcept = 0;
    virtual const char* Name() const noexcept { return "<ufsm::state>"; }

    virtual ~StateBase() = default;

protected:
    StateBase() = default;

private:
    template <typename, typename>
    friend class ::ufsm::StateMachine;

    std::size_t ActiveIndex() const noexcept { return active_index_; }
    void SetActiveIndex(std::size_t index) noexcept { active_index_ = index; }

    std::size_t active_index_ = 0;
};


} // namespace detail

// Public API surface:
// - `ufsm::Result` follows Google C++ style (type names are CamelCase).
// - helpers `forward_event()`, `discard_event()`, `consume_event()`.
// Note: `Result::kDeferEvent` is used internally by `State::defer_event()`.
// Prefer these helpers in user code (similar to boost::statechart).
[[nodiscard]] constexpr Result forward_event() noexcept { return Result::kForwardEvent; }
[[nodiscard]] constexpr Result discard_event() noexcept { return Result::kDiscardEvent; }
[[nodiscard]] constexpr Result consume_event() noexcept { return Result::kConsumed; }

template <typename Derived>
class Event : public detail::EventBase {
public:
    Event() noexcept : detail::EventBase(StaticTypeId()) {}

    const char* Name() const noexcept override {
        return detail::PrettyTypeNameCStr<Derived>();
    }

    detail::EventBase::Ptr Clone() const override {
        static_assert(std::is_copy_constructible_v<Derived>,
                      "Events must be copy constructible to be deferred as ufsm::detail::EventBase.");
        return detail::EventBase::Ptr(new Derived(static_cast<const Derived&>(*this)));
    }

    static const void* StaticTypeId() noexcept {
        static const int tag = 0;
        return &tag;
    }
};

// Event reaction wrapper for dispatching specific event types to state's React method
template <class EventType>
class Reaction {
public:
    template <typename StateType, typename EventBaseType>
    static Result React(StateType& state, const EventBaseType& event) {
        static_assert(std::is_base_of_v<detail::EventBase, EventType>,
                      "EventType must derive from ufsm::Event<EventType> (i.e., from ufsm::detail::EventBase).");

        static_assert(detail::HasReactMethod<StateType, EventType>::value,
                      "StateType must implement React(const EventType&) (returning void or ufsm::Result).");

        using ReturnT = typename detail::ReactReturnType<StateType, EventType>::type;
        static_assert(std::is_void_v<ReturnT> || std::is_same_v<ReturnT, ufsm::Result>,
                      "React(const EventType&) must return void or ufsm::Result.");

        // RTTI-free match: compare type identity, then static_cast.
        if (event.TypeId() == EventType::StaticTypeId()) {
            const auto& casted_event = static_cast<const EventType&>(event);
            // If state's React returns void, assume event is consumed by default
            if constexpr (std::is_void_v<decltype(state.React(casted_event))>) {
                state.React(casted_event);
                return Result::kConsumed;
            } else {
                // Otherwise return the result from state's React method
                return state.React(casted_event);
            }
        }
        return Result::kNoReaction;
    }
};

// Boost.Statechart-like declarative transition entry.
// Usage in a state:
//   using reactions = ufsm::mp::List<
//     ufsm::Transition<MyEvent, NextState>,
//     ufsm::Transition<MyEvent2, NextState2, MyAction>,
//     ufsm::Reaction<OtherEvent>
//   >;
// This allows simple "event -> next state" without defining a React() function.
// If Action is provided, it is default-constructed and executed on the least common
// ancestor context, using the same rules as Transit<Dest>(action):
//   - action(CommonCtx&) is preferred when applicable
//   - otherwise action() is accepted
template <class EventType, class DestState, class Action = std::nullptr_t>
class Transition {
public:
    template <typename StateType, typename EventBaseType>
    static Result React(StateType& state, const EventBaseType& event) {
        static_assert(std::is_base_of_v<detail::EventBase, EventType>,
                      "EventType must derive from ufsm::Event<EventType> (i.e., from ufsm::detail::EventBase).\n");

        if (event.TypeId() == EventType::StaticTypeId()) {
            (void)static_cast<const EventType&>(event);
            if constexpr (std::is_same_v<Action, std::nullptr_t>) {
                return state.template Transit<DestState>();
            } else {
                static_assert(std::is_default_constructible_v<Action>,
                              "ufsm::Transition<...> with Action requires Action to be default constructible.");
                return state.template Transit<DestState>(Action{});
            }
        }
        return Result::kNoReaction;
    }
};

// Optional Boost-like alias name (lowercase) for readability.
template <class EventType, class DestState, class Action = std::nullptr_t>
using transition = Transition<EventType, DestState, Action>;

// Boost.Statechart-like deferral reaction entry.
// Usage in a state:
//   using reactions = ufsm::List<ufsm::Deferral<MyEvent>>;
template <class EventType>
class Deferral {
public:
    template <typename StateType, typename EventBaseType>
    static Result React(StateType& state, const EventBaseType& event) {
        static_assert(std::is_base_of_v<detail::EventBase, EventType>,
                      "EventType must derive from ufsm::Event<EventType> (i.e., from ufsm::detail::EventBase).\n");

        if constexpr (std::is_same_v<EventType, detail::EventBase>) {
            return state.defer_event();
        } else {
            if (event.TypeId() == EventType::StaticTypeId()) {
                return state.defer_event();
            }
            return Result::kNoReaction;
        }
    }
};

template <class EventType>
using deferral = Deferral<EventType>;

namespace detail {

template <typename MachineType, typename = void>
struct HasOnUnhandledEventMethod : std::false_type {};

template <typename MachineType>
struct HasOnUnhandledEventMethod<MachineType,
                                 std::void_t<decltype(std::declval<MachineType&>().OnUnhandledEvent(
                                     std::declval<const EventBase&>()))>> : std::true_type {};

template <typename MachineType>
inline void InvokeOnUnhandledEventIfPresent(MachineType& machine, const EventBase& event) {
    if constexpr (HasOnUnhandledEventMethod<MachineType>::value) {
        machine.OnUnhandledEvent(event);
    } else {
        (void)machine;
        (void)event;
    }
}

template <typename EventType, typename DestState, typename Action>
struct IsTransition<Transition<EventType, DestState, Action>> : std::true_type {};

template <typename EventType>
struct IsDeferral<Deferral<EventType>> : std::true_type {};

} // namespace detail

namespace detail {

// State constructor: Recursively construct state hierarchy
template <class ContextList, class OutermostContext>
struct Constructor;

template <typename Head, typename... Tail, class OutermostContext>
struct Constructor<mp::List<Head, Tail...>, OutermostContext> {
    template <typename ContextPtr>
    static void Construct(const ContextPtr& p_context, OutermostContext& out_context) {
        if constexpr (sizeof...(Tail) == 0) {
            // Last state, perform deep construction (including its substates)
            Head::DeepConstruct(p_context, out_context);
        } else {
            // Shallow construct current state, then recursively construct remaining states
            auto p = Head::ShallowConstruct(p_context, out_context);
            Constructor<mp::List<Tail...>, OutermostContext>::Construct(p, out_context);
        }
    }
};

template <class CurrentContext, class DestState>
struct MakeContextList {
    using type = typename mp::PushBack<typename mp::BuildPath<typename DestState::ContextTypeList, CurrentContext>::type, DestState>::type;
};

} // namespace detail

// State template class
// Derived: Derived state type (CRTP pattern)
// ContextState: Parent state type
// InnerInitial: Initial substate type (void indicates no substates)
template <typename Derived, typename ContextState, typename InnerInitial = void>
class State : public detail::StateBase {
public:
    using InnerInitialType = InnerInitial;
    using reactions = mp::List<>;                                           // Event reaction list (overridden by derived class)
    using ContextType = typename ContextState::InnerContextType;            // Parent state type
    using ContextPtrType = typename ContextType::InnerContextPtrType;       // Parent context pointer type (raw, non-owning)
    using OutermostContextType = typename ContextType::OutermostContextType;// Outermost state machine type
    using InnerContextType = Derived;                                       // This state as inner context type
    using InnerContextPtrType = Derived*;                                   // This state's pointer type
    using OutermostContextBaseType = typename ContextType::OutermostContextBaseType; // Outermost state machine base class
    using ContextTypeList = typename mp::PushFront<typename ContextType::ContextTypeList, ContextType>::type; // Context type list

    OutermostContextType& OutermostContext() {
        return p_context_->OutermostContext();
    }

    const OutermostContextType& OutermostContext() const {
        return p_context_->OutermostContext();
    }

    // Boost.Statechart-like helper: allow states to post deferred events without
    // reaching through OutermostContext().
    template <class Ev>
    void PostEvent(Ev&& event) {
        OutermostContext().PostEvent(std::forward<Ev>(event));
    }

    // Boost.Statechart-like naming convenience: defers the *currently handled* event.
    // Intended to be called from inside a reaction handler.
    [[nodiscard]] Result defer_event() {
        UFSM_ASSERT(OutermostContextBase().CurrentEventPtr() != nullptr);
        deferred_flag_ = true;
        return Result::kDeferEvent;
    }

    template <class TargetContext>
    TargetContext& Context() {
        if constexpr (std::is_base_of_v<TargetContext, Derived>) {
            return *static_cast<Derived*>(this);
        } else {
            return p_context_->template Context<TargetContext>();
        }
    }

    template <class TargetContext>
    const TargetContext& Context() const {
        if constexpr (std::is_base_of_v<TargetContext, Derived>) {
            return *static_cast<const Derived*>(this);
        } else {
            return p_context_->template Context<TargetContext>();
        }
    }

    // Transition with action executed on the least common ancestor (common parent) context.
    // Supports two action shapes:
    //  - action(CommonCtx&)
    //  - action()
    template <class DestState, class Action = std::nullptr_t>
    [[nodiscard]] Result Transit(Action&& action = nullptr) {
        return TransitImpl<DestState>(std::forward<Action>(action));
    }

    const void* TypeId() const noexcept override {
        return StaticTypeId();
    }

    const char* Name() const noexcept override {
        return detail::PrettyTypeNameCStr<Derived>();
    }

    static const void* StaticTypeId() noexcept {
        static const int tag = 0;
        return &tag;
    }

protected:
    State() : p_context_(nullptr) {}

    State(ContextType& context) : p_context_(nullptr) {
        p_context_ = &context;
    }

    ~State() {
        if (p_context_ != nullptr && deferred_flag_) {
            // Boost.Statechart releases deferred events when a deferring state exits.
            OutermostContextBase().ReleaseDeferredEvents();
        }
    }

private:
    void SetContext(ContextPtrType p_context) {
        static_assert(std::is_pointer_v<ContextPtrType>,
                      "ufsm (scheme 2): ContextPtrType is expected to be a raw pointer type.");
        p_context_ = p_context;
    }

    // State transition implementation
    // DestState: Target state type
    // Action: Transition action (executed on common parent state)
    template <class DestState, class Action>
    [[nodiscard]] Result TransitImpl(Action&& action) {
        // Find the least common ancestor of source and destination states
        using CommonCtx = typename mp::FindFirstCommon<ContextTypeList, typename DestState::ContextTypeList>::type;
        // Find the direct child state of common parent (state to be terminated)
        using TermState = typename mp::FindChildOf<typename mp::PushFront<ContextTypeList, Derived>::type, CommonCtx>::type;

        // Get the state to terminate and common parent state
        TermState& term_state(Context<TermState>());
        CommonCtx& common_ctx = term_state.template Context<CommonCtx>();
        auto* p_common = &common_ctx;
        auto& out_base = common_ctx.OutermostContextBase();

#if !defined(NDEBUG)
        UFSM_ASSERT(!out_base.ufsm_in_transition_);
        out_base.ufsm_in_transition_ = true;
        struct UfsmTransitionGuard {
            bool* f;
            ~UfsmTransitionGuard() { *f = false; }
        } guard{&out_base.ufsm_in_transition_};
#endif

        // 1. Terminate all substates from term_state to current state
        out_base.TerminateAsPartOfTransit(term_state);
        // 2. Execute transition action on common parent state
        InvokeTransitionAction(std::forward<Action>(action), common_ctx);
        // 3. Construct the path from common parent to destination state
        detail::Constructor<typename detail::MakeContextList<CommonCtx, DestState>::type, OutermostContextBaseType>::Construct(
            p_common, out_base);

        return Result::kConsumed;
    }

    template <typename... Reactions>
    Result LocalReact(const detail::EventBase& event, mp::List<Reactions...>) {
        Result res = Result::kNoReaction;
        (void)((res = Reactions::React(*static_cast<Derived*>(this), event),
                res != Result::kNoReaction) ||
               ...);
        return res == Result::kNoReaction ? Result::kForwardEvent : res;
    }

    // Internal plumbing used by transitions/deferral.
    OutermostContextBaseType& OutermostContextBase() {
        return p_context_->OutermostContextBase();
    }

    Result ReactImpl(const detail::EventBase& event) override {
        StaticChecks();
        auto res = LocalReact(event, typename Derived::reactions{});
        if (res == Result::kForwardEvent) {
            return p_context_->ReactImpl(event);
        }
        return res;
    }

    template <typename, typename, typename>
    friend class ufsm::State;
    template <class, class>
    friend class ufsm::StateMachine;
    template <class, class>
    friend struct ufsm::detail::Constructor;

    static void DeepConstruct(const ContextPtrType& p_context, OutermostContextBaseType& out_context) {
        auto* p_inner = ShallowConstruct(p_context, out_context);
        if constexpr (!std::is_same_v<void, InnerInitial>) {
            InnerInitial::DeepConstruct(p_inner, out_context);
        }
    }

    static InnerContextPtrType ShallowConstruct(const ContextPtrType& p_context, OutermostContextBaseType& out_context) {
        static_assert(std::is_pointer_v<ContextPtrType>,
                      "ufsm (scheme 2): ContextPtrType is expected to be a raw pointer type.");
        std::unique_ptr<Derived> p_inner;
        if constexpr (std::is_constructible_v<Derived, ContextType&>) {
            p_inner = std::make_unique<Derived>(*p_context);
        } else {
            p_inner = std::make_unique<Derived>();
            p_inner->SetContext(p_context);
        }
        return out_context.Add(std::move(p_inner), p_context);
    }

    static void StaticChecks() {
        static_assert(std::is_base_of_v<State<Derived, ContextState, InnerInitial>, Derived>,
                      "Derived must inherit from ufsm::State<Derived, ContextState, InnerInitial> (CRTP).");
        static_assert(detail::IsMpList<typename Derived::reactions>::value,
                      "Derived::reactions must be ufsm::mp::List<...>.");
        static_assert(detail::AllAreReactionEntries<typename Derived::reactions>::value,
                  "Derived::reactions must contain only ufsm::Reaction<...>, ufsm::Transition<...,...>, or ufsm::Deferral<...> entries.");
    }

    // Invoke transition action with preference for action(CommonCtx&) over action().
    template <class Action, class CommonCtx>
    static void InvokeTransitionAction(Action&& action, CommonCtx& common_ctx) {
        using ActionT = std::remove_reference_t<Action>;
        if constexpr (std::is_same_v<ActionT, std::nullptr_t>) {
            (void)action;
            (void)common_ctx;
        } else if constexpr (std::is_invocable_v<Action&&, CommonCtx&>) {
            std::forward<Action>(action)(common_ctx);
        } else if constexpr (std::is_invocable_v<Action&&>) {
            std::forward<Action>(action)();
        } else {
            static_assert(std::is_invocable_v<Action&&, CommonCtx&> || std::is_invocable_v<Action&&>,
                          "Transition action must be invocable either as action(CommonCtx&) or action().");
        }
    }

    // Non-owning pointer to the parent context.
    // Valid only while this state is active; do not cache across transitions.
    ContextPtrType p_context_;

    bool deferred_flag_ = false;
};

// State machine template class
// Derived: Derived state machine type (CRTP pattern)
// InnerInitial: Initial state type
template <typename Derived, typename InnerInitial>
class StateMachine {
public:
    using InnerContextType = Derived;               // This state machine as inner context type
    using OutermostContextType = Derived;           // Outermost context (the state machine itself)
    using OutermostContextBaseType = StateMachine;  // Outermost base class
    using InnerContextPtrType = StateMachine*;      // Root context pointer type (raw, non-owning)
    using ContextTypeList = mp::List<>;             // Empty context list (state machine is root)
    using ContextType = void;                       // No parent context

    void Initiate() {
        static_assert(std::is_base_of_v<StateMachine<Derived, InnerInitial>, Derived>,
                      "Derived must inherit from ufsm::StateMachine<Derived, InnerInitial> (CRTP).");
        static_assert(std::is_base_of_v<detail::StateBase, InnerInitial>,
                      "InnerInitial must be a ufsm state type (derive from ufsm::State<...>)." );
        TerminateImpl();
        InnerInitial::DeepConstruct(static_cast<Derived*>(this), *static_cast<Derived*>(this));
    }

    void Terminate() {
#if !defined(NDEBUG)
        UFSM_ASSERT(!ufsm_in_transition_);
#endif
        TerminateImpl();
    }

    bool Terminated() const noexcept {
        return !current_state_;
    }

    template <class Ev>
    Result ProcessEvent(const Ev& event) {
        static_assert(std::is_base_of_v<detail::EventBase, Ev>,
                      "Event type must derive from ufsm::Event<Ev> (i.e., from ufsm::detail::EventBase).");
        static_assert(!std::is_same_v<std::decay_t<Ev>, detail::EventBase>,
                      "ProcessEvent expects a concrete event type (derive from ufsm::Event<Derived>), not ufsm::detail::EventBase.");
        return ProcessEventLoop(event);
    }

    // Boost-like posted events.
    // PostEvent() enqueues an event to be processed after the current ProcessEvent()
    // completes (the event loop drains the posted queue automatically).
    // This is useful to avoid re-entrancy when an event handler wants to trigger follow-up events.
    template <class Ev>
    void PostEvent(Ev&& event) {
        static_assert(std::is_base_of_v<detail::EventBase, std::decay_t<Ev>>,
                      "Event type must derive from ufsm::Event<Ev> (i.e., from ufsm::detail::EventBase)." );
        // Simpler but potentially more alloc-heavy: always clone into EventBase::Ptr.
        posted_events_.push_back(static_cast<const detail::EventBase&>(event).Clone());
    }

    // State queries (Boost.Statechart-like).
    // Returns a pointer to an active state of type StateT if it is currently in the active chain,
    // otherwise nullptr.
    template <class StateT>
    StateT* StatePtr() {
        return const_cast<StateT*>(static_cast<const StateMachine*>(this)->template StatePtr<StateT>());
    }

    template <class StateT>
    const StateT* StatePtr() const {
        static_assert(std::is_base_of_v<detail::StateBase, StateT>,
                      "StateT must be a ufsm state type (derive from ufsm::State<...>)." );

        // Linear scan over the active chain (typically shallow).
        if (!active_path_.empty()) {
            for (const auto& p_state : active_path_) {
                if (p_state && p_state->TypeId() == StateT::StaticTypeId()) {
                    return static_cast<const StateT*>(p_state.get());
                }
            }
        }
        return nullptr;
    }

    template <class StateT>
    bool IsInState() const {
        return this->template StatePtr<StateT>() != nullptr;
    }

    template <class TargetContext>
    TargetContext& Context() {
        return *static_cast<Derived*>(this);
    }

    template <class TargetContext>
    const TargetContext& Context() const {
        return *static_cast<const Derived*>(this);
    }

    OutermostContextType& OutermostContext() {
        return *static_cast<Derived*>(this);
    }

    const OutermostContextType& OutermostContext() const {
        return *static_cast<const Derived*>(this);
    }

protected:
    StateMachine() = default;

    virtual ~StateMachine() {
        TerminateImpl();
    }

private:
    template <typename, typename, typename>
    friend class ufsm::State;

    OutermostContextBaseType& OutermostContextBase() {
        return *this;
    }

    void TerminateAsPartOfTransit(detail::StateBase& state) {
        TerminateImpl(state);
    }

#if !defined(NDEBUG)
    bool ufsm_in_transition_ = false;
#endif

    Result ReactImpl(const detail::EventBase&) {
        return Result::kForwardEvent;
    }

    // Add state to the state machine.
    // Called during state construction to track and own the currently active state chain.
    template <class StateType, class ParentType>
    StateType* Add(std::unique_ptr<StateType> p_state, const ParentType& parent) {
        (void)parent;
        constexpr bool is_leaf = std::is_same_v<typename StateType::InnerInitialType, void>;

        p_state->SetActiveIndex(active_path_.size());
        StateType* const raw = p_state.get();
        active_path_.push_back(std::move(p_state));

        if constexpr (is_leaf) {
            current_state_ = raw;
        }

        return raw;
    }

    Result ProcessEventLoop(const detail::EventBase& event) {
#if !defined(NDEBUG)
        UFSM_ASSERT(!ufsm_in_transition_);
#endif
        // Boost-like behavior: when ProcessEvent returns, all posted events
        // have been dispatched (FIFO), preventing re-entrant recursion.
        if (ufsm_in_event_loop_) {
            return ProcessEventImpl(event);
        }

        ufsm_in_event_loop_ = true;
        struct UfsmEventLoopGuard {
            bool* f;
            ~UfsmEventLoopGuard() { *f = false; }
        } guard{&ufsm_in_event_loop_};

        Result last = ProcessEventImpl(event);
        while (!posted_events_.empty()) {
            auto ev = std::move(posted_events_.front());
            posted_events_.pop_front();
            UFSM_ASSERT(ev != nullptr);
            // We're already in the event loop; directly dispatch without re-entering
            // the loop/drain logic.
            last = ProcessEventImpl(*ev);
        }
        return last;
    }

    const detail::EventBase* CurrentEventPtr() const noexcept {
        return current_event_;
    }

    void ReleaseDeferredEvents() {
        PrependPostedEvents(deferred_events_);
    }

    void PrependPostedEvents(std::deque<detail::EventBase::Ptr>& events) {
        if (events.empty()) return;
        while (!events.empty()) {
            posted_events_.push_front(std::move(events.back()));
            events.pop_back();
        }
    }

    Result ProcessEventImpl(const detail::EventBase& event) {
        const detail::EventBase* prev_event = current_event_;
        current_event_ = &event;
        struct UfsmCurrentEventGuard {
            const detail::EventBase** slot;
            const detail::EventBase* prev;
            ~UfsmCurrentEventGuard() { *slot = prev; }
        } current_event_guard{&current_event_, prev_event};

        const auto res = SendEvent(event);

        if (res == Result::kDeferEvent) {
            deferred_events_.push_back(event.Clone());
            return Result::kConsumed;
        }

        // If the event bubbles all the way to the root and is still forwarded,
        // treat it as unhandled. Users can optionally define:
        //   void OnUnhandledEvent(const ufsm::detail::EventBase&)
        // on the state machine to observe/log/trace.
        if (res == Result::kForwardEvent) {
            detail::InvokeOnUnhandledEventIfPresent(*static_cast<Derived*>(this), event);
        }
        return res;
    }

    // Keeps the outermost-to-innermost prefix of size keep_count and destroys the rest.
    // This provides deterministic destruction order (innermost first).
    void ResetToDepth(std::size_t keep_count) {
        if (keep_count > active_path_.size()) keep_count = active_path_.size();
        while (active_path_.size() > keep_count) {
            active_path_.pop_back();
        }

        current_state_ = active_path_.empty() ? nullptr : active_path_.back().get();
    }

    Result SendEvent(const detail::EventBase& event) {
        if (!current_state_) return Result::kForwardEvent;
        return current_state_->ReactImpl(event);
    }

    void TerminateImpl() {
        ResetToDepth(0);
        posted_events_.clear();
        deferred_events_.clear();
    }

    // Termination used by transition boundary semantics.
    // Destroys states from the current leaf up to and including the given boundary state.
    // The parent of the boundary remains alive and becomes the construction anchor.
    void TerminateImpl(detail::StateBase& state) {
        if (active_path_.empty()) {
            current_state_ = nullptr;
            return;
        }

        // Fast path: states record their active_path index.
        const std::size_t boundary_state_index = state.ActiveIndex();
        UFSM_ASSERT(boundary_state_index < active_path_.size());
        UFSM_ASSERT(active_path_[boundary_state_index].get() == &state);

        // Release fallback: avoid linear scans; a mismatch indicates a logic bug.
        if (boundary_state_index >= active_path_.size() || active_path_[boundary_state_index].get() != &state) {
            ResetToDepth(0);
            return;
        }

        // If not found, fall back to full termination.
        // Keep only the states strictly above the boundary state.
        const std::size_t keep_count = boundary_state_index;
        ResetToDepth(keep_count);
    }

    detail::StateBase* current_state_ = nullptr; // Current active innermost (leaf) state
    // Outermost-to-innermost active states.
    // Ownership is centralized here; states reference their parent via raw pointer.
    std::vector<std::unique_ptr<detail::StateBase>> active_path_;

    const detail::EventBase* current_event_ = nullptr;

    // FIFO queue of deferred events (Boost.Statechart-like).
    std::deque<detail::EventBase::Ptr> deferred_events_;

    // FIFO queue of posted events.
    std::deque<detail::EventBase::Ptr> posted_events_;

    // Prevents recursive event loops; used to implement boost-like posted-event semantics.
    bool ufsm_in_event_loop_ = false;

};

} // namespace ufsm

#define FSM_STATE_MACHINE(Name, InnerInitial) \
    struct InnerInitial;                   \
    struct Name : ufsm::StateMachine<Name, InnerInitial>

#define _FSM_STATE_2(Name, Context) \
    struct Context;              \
    struct Name : ufsm::State<Name, Context>

#define _FSM_STATE_3(Name, Context, InnerInitial) \
    struct Context;                 \
    struct InnerInitial;                 \
    struct Name : ufsm::State<Name, Context, InnerInitial>

#define FSM_STATE(...) _FSM_GET_MACRO(__VA_ARGS__, _FSM_STATE_3, _FSM_STATE_2)(__VA_ARGS__)

#define _FSM_GET_MACRO(_1, _2, _3, NAME, ...) NAME

#define FSM_EVENT(Name) struct Name : ufsm::Event<Name>
