#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

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

// Event handling result types.
enum class Result {
    kNoReaction,   // Event not handled
    kForwardEvent, // Forward event to parent state
    kDiscardEvent, // Discard event (actively ignored)
    kConsumed      // Event consumed (handled)
};

namespace detail {

template <typename T>
struct IsMpList : std::false_type {};

template <typename... Ts>
struct IsMpList<mp::List<Ts...>> : std::true_type {};

class EventBase {
protected:
    virtual ~EventBase() = default;

    constexpr explicit EventBase(const void* type_id) noexcept : type_id_(type_id) {}

public:
    // Type identity for RTTI-free event dispatch.
    // The returned pointer is unique per concrete event type.
    const void* TypeId() const noexcept { return type_id_; }

private:
    const void* type_id_;
};

// Base class for all states, providing common interface
// Inherits enable_shared_from_this to support creating shared_ptr from this pointer
class StateBase : public std::enable_shared_from_this<StateBase> {
public:
    using Ptr = std::shared_ptr<StateBase>;

    virtual Result ReactImpl(const EventBase& event) = 0;
    virtual void ExitImpl(Ptr& p_self, Ptr& p_outermost_unstable_state, bool perform_full_exit) = 0;

protected:
    StateBase() = default;
    virtual ~StateBase() = default;
};

} // namespace detail

// Public API surface:
// - `ufsm::Result` follows Google C++ style (type names are CamelCase).
// - helpers `forward_event()`, `discard_event()`, `consume_event()`.
// Prefer these helpers in user code (similar to boost::statechart).
[[nodiscard]] constexpr Result forward_event() noexcept { return Result::kForwardEvent; }
[[nodiscard]] constexpr Result discard_event() noexcept { return Result::kDiscardEvent; }
[[nodiscard]] constexpr Result consume_event() noexcept { return Result::kConsumed; }

template <typename Derived>
class Event : public detail::EventBase {
public:
    Event() noexcept : detail::EventBase(StaticTypeId()) {}

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
    using ContextPtrType = typename ContextType::InnerContextPtrType;       // Parent state pointer type
    using OutermostContextType = typename ContextType::OutermostContextType;// Outermost state machine type
    using InnerContextType = Derived;                                       // This state as inner context type
    using InnerContextPtrType = std::shared_ptr<Derived>;                  // This state's pointer type
    using OutermostContextBaseType = typename ContextType::OutermostContextBaseType; // Outermost state machine base class
    using ContextTypeList = typename mp::PushFront<typename ContextType::ContextTypeList, ContextType>::type; // Context type list

    OutermostContextType& OutermostContext() {
        return p_context_->OutermostContext();
    }

    const OutermostContextType& OutermostContext() const {
        return p_context_->OutermostContext();
    }

    OutermostContextBaseType& OutermostContextBase() {
        return p_context_->OutermostContextBase();
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

    static void DeepConstruct(const ContextPtrType& p_context, OutermostContextBaseType& out_context) {
        auto p_inner = ShallowConstruct(p_context, out_context);
        if constexpr (!std::is_same_v<void, InnerInitial>) {
            InnerInitial::DeepConstruct(p_inner, out_context);
        }
    }

    static InnerContextPtrType ShallowConstruct(const ContextPtrType& p_context, OutermostContextBaseType& out_context) {
        InnerContextPtrType p_inner;
        if constexpr (std::is_constructible_v<Derived, ContextType&>) {
            if constexpr (std::is_pointer_v<ContextPtrType>) {
                p_inner = std::make_shared<Derived>(*static_cast<ContextType*>(p_context));
            } else {
                p_inner = std::make_shared<Derived>(*p_context);
            }
        } else {
            p_inner = std::make_shared<Derived>();
            p_inner->SetContext(p_context);
        }
        out_context.Add(p_inner, p_context);
        return p_inner;
    }

    template <class TargetContext>
    const typename TargetContext::InnerContextPtrType& ContextPtr() const {
        if constexpr (std::is_same_v<TargetContext, ContextType>) {
            return p_context_;
        } else {
            return p_context_->template ContextPtr<TargetContext>();
        }
    }

    void SetOutermostUnstableState(Ptr& p_outer_unstable) {
        p_outer_unstable = shared_from_this();
    }

    // State exit implementation
    // Uses reference counting to determine state activity and avoid duplicate exits
    void ExitImpl(InnerContextPtrType& p_self, Ptr& p_outer_unstable, bool perform_full_exit) {
        long use_count = weak_from_this().use_count();

        // Skip if state is being handled by another exit flow
        if (use_count > 2) return;

        // If this is the outermost unstable state, notify parent
        if (use_count == 2 && p_outer_unstable.get() == this) {
            p_context_->SetOutermostUnstableState(p_outer_unstable);
            return;
        }

        // Normal exit: use_count <= 2 and not outermost
        // For full termination, do not auto-establish an "unstable" boundary.
        if (!perform_full_exit && !p_outer_unstable) {
            p_context_->SetOutermostUnstableState(p_outer_unstable);
        }
        ContextPtrType p_ctx = p_context_;
        p_self = nullptr;
        p_ctx->ExitImpl(p_ctx, p_outer_unstable, perform_full_exit);
    }

    Result ReactImpl(const detail::EventBase& event) override {
        StaticChecks();
        auto res = LocalReact(event, typename Derived::reactions{});
        if (res == Result::kForwardEvent) {
            return p_context_->ReactImpl(event);
        }
        return res;
    }

protected:
    State() : p_context_(nullptr) {}

    State(ContextType& context) : p_context_(nullptr) {
        if constexpr (std::is_pointer_v<ContextPtrType>) {
            p_context_ = &context;
        } else {
            p_context_ = std::static_pointer_cast<ContextType>(context.shared_from_this());
        }
    }

    ~State() = default;

    void SetContext(const ContextPtrType& p_context) {
        p_context_ = p_context;
    }

    void ExitImpl(Ptr& p_self, Ptr& p_outer_unstable, bool perform_full_exit) override {
        InnerContextPtrType p_derived = std::static_pointer_cast<Derived>(shared_from_this());
        p_self = nullptr;
        ExitImpl(p_derived, p_outer_unstable, perform_full_exit);
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
        const auto p_common = term_state.template ContextPtr<CommonCtx>();
        CommonCtx& common_ctx = term_state.template Context<CommonCtx>();
        auto& out_base = common_ctx.OutermostContextBase();

        // 1. Terminate all substates from term_state to current state
        out_base.TerminateAsPartOfTransit(term_state);
        // 2. Execute transition action on common parent state
        InvokeTransitionAction(std::forward<Action>(action), common_ctx);
        // 3. Construct the path from common parent to destination state
        detail::Constructor<typename detail::MakeContextList<CommonCtx, DestState>::type, OutermostContextBaseType>::Construct(p_common, out_base);

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

private:
    static void StaticChecks() {
        static_assert(std::is_base_of_v<State<Derived, ContextState, InnerInitial>, Derived>,
                      "Derived must inherit from ufsm::State<Derived, ContextState, InnerInitial> (CRTP).");
        static_assert(detail::IsMpList<typename Derived::reactions>::value,
                      "Derived::reactions must be ufsm::mp::List<...>.");
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

    ContextPtrType p_context_;
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
    using InnerContextPtrType = StateMachine*;      // State machine uses raw pointer (no shared ownership needed)
    using ContextTypeList = mp::List<>;             // Empty context list (state machine is root)
    using ContextType = void;                       // No parent context

    void Initiate() {
        static_assert(std::is_base_of_v<StateMachine<Derived, InnerInitial>, Derived>,
                      "Derived must inherit from ufsm::StateMachine<Derived, InnerInitial> (CRTP).");
        static_assert(std::is_base_of_v<detail::StateBase, InnerInitial>,
                      "InnerInitial must be a ufsm state type (derive from ufsm::State<...>)." );
        TerminateImpl(true);
        InnerInitial::DeepConstruct(static_cast<Derived*>(this), *static_cast<Derived*>(this));
    }

    void Terminate() {
        TerminateImpl(true);
    }

    bool Terminated() const noexcept {
        return !current_state_;
    }

    template <class Ev>
    void ProcessEvent(const Ev& event) {
        static_assert(std::is_base_of_v<detail::EventBase, Ev>,
                      "Event type must derive from ufsm::Event<Ev> (i.e., from ufsm::detail::EventBase).");
        SendEvent(event);
    }

    void ExitImpl(InnerContextPtrType&, detail::StateBase::Ptr& p_outer_unstable, bool) {
        p_outer_unstable = nullptr;
    }

    void SetOutermostUnstableState(detail::StateBase::Ptr& p_state) {
        p_state = nullptr;
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

    OutermostContextBaseType& OutermostContextBase() {
        return *this;
    }

    void TerminateAsPartOfTransit(detail::StateBase& state) {
        TerminateImpl(state);
        is_innermost_common_outer_ = true;
    }

    Result ReactImpl(const detail::EventBase&) {
        return Result::kForwardEvent;
    }

    // Add state to state machine
    // Called during state construction to track current active state and unstable states
    template <class StateType, class ParentType>
    void Add(const std::shared_ptr<StateType>& p_state, const ParentType& parent) {
        constexpr bool is_leaf = std::is_same_v<typename StateType::InnerInitialType, void>;

        detail::StateBase::Ptr p_candidate = is_leaf ? nullptr : p_state;

        if constexpr (is_leaf) {
            current_state_ = p_state;
        }

        // Update outermost unstable state if needed
        bool is_parent_unstable;
        if constexpr (std::is_pointer_v<ParentType>) {
            is_parent_unstable = (p_outermost_unstable_state_ == nullptr);
        } else {
            is_parent_unstable = (p_outermost_unstable_state_ == parent);
        }

        if (is_innermost_common_outer_ || is_parent_unstable) {
            is_innermost_common_outer_ = false;
            p_outermost_unstable_state_ = p_candidate;
        }
    }

protected:
    StateMachine()
        : is_innermost_common_outer_(false),
          p_outermost_unstable_state_(nullptr) {}

    virtual ~StateMachine() {
        TerminateImpl(true);
        current_state_.reset();  // Explicitly release state before derived class destructs
    }

private:
    Result SendEvent(const detail::EventBase& event) {
        if (!current_state_) return Result::kForwardEvent;
        return current_state_->ReactImpl(event);
    }

    void TerminateImpl(bool perform_full_exit) {
        if (current_state_) {
            auto p = current_state_;
            current_state_ = nullptr;
            p->ExitImpl(p, p_outermost_unstable_state_, perform_full_exit);
        }
    }

    // State machine termination implementation (with state parameter)
    // Terminates all states from current state up to (but not including) the parent of 'state'
    void TerminateImpl(detail::StateBase& state) {
        is_innermost_common_outer_ = false;

        if (current_state_) {
            // Set state as the termination boundary
            p_outermost_unstable_state_ = state.shared_from_this();

            // Exit from current state
            auto p = current_state_;
            current_state_ = nullptr;
            // For transitions, we still want to fully unwind until the boundary state.
            p->ExitImpl(p, p_outermost_unstable_state_, true);

            // Clear the boundary marker
            p_outermost_unstable_state_ = nullptr;
        }
    }

    detail::StateBase::Ptr current_state_;              // Current active innermost state
    bool is_innermost_common_outer_;                    // Flag for innermost common outer state
    detail::StateBase::Ptr p_outermost_unstable_state_; // Outermost unstable state (during transition)
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
