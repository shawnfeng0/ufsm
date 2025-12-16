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
template <typename... Types> struct List {};
template <typename ListType, typename Type> struct PushFront;
template <typename... Types, typename Type> struct PushFront<List<Types...>, Type> { using type = List<Type, Types...>; };
template <typename ListType, typename Type> struct PushBack;
template <typename... Types, typename Type> struct PushBack<List<Types...>, Type> { using type = List<Types..., Type>; };
template <typename ListType, typename Type> struct Contains : std::false_type {};
template <typename... Types, typename Type> struct Contains<List<Types...>, Type> : std::bool_constant<(std::is_same_v<Types, Type> || ...)> {};
template <typename ListType, template <typename> class Pred> struct FindIf { using type = void; };
template <typename Head, typename... Tail, template <typename> class Pred>
struct FindIf<List<Head, Tail...>, Pred> { using type = std::conditional_t<Pred<Head>::value, Head, typename FindIf<List<Tail...>, Pred>::type>; };
template <typename List1, typename List2>
struct FindFirstCommon { template <typename T> using Pred = Contains<List2, T>; using type = typename FindIf<List1, Pred>::type; };
template <typename ListType, typename ParentType>
struct FindChildOf { template <typename T> using Pred = std::is_same<typename T::ContextType, ParentType>; using type = typename FindIf<ListType, Pred>::type; };
template <typename ListType, typename StopType> struct BuildPath { using type = List<>; };
template <typename Head, typename... Tail, typename StopType>
struct BuildPath<List<Head, Tail...>, StopType> { using type = std::conditional_t<std::is_same_v<Head, StopType>, List<>, typename PushBack<typename BuildPath<List<Tail...>, StopType>::type, Head>::type>; };
} // namespace mp
template <typename... Types>
using List = mp::List<Types...>;
enum class Result { kNoReaction, kForwardEvent, kDiscardEvent, kDeferEvent, kConsumed };
template <class EventType> class Reaction;
template <class EventType, class DestState, class Action> class Transition;
template <class EventType> class Deferral;
template <typename Derived, typename InnerInitial> class StateMachine;
namespace detail {
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
class StateBase {
public:
    virtual Result ReactImpl(const EventBase& event) = 0;
    virtual const void* TypeId() const noexcept = 0;
    virtual const char* Name() const noexcept { return "<ufsm::state>"; }
    virtual ~StateBase() = default;
    std::size_t ActiveIndex() const noexcept { return active_index_; }
    void SetActiveIndex(std::size_t index) noexcept { active_index_ = index; }
protected:
    StateBase() = default;
private:
    std::size_t active_index_ = 0;
};
template <typename MachineType, typename = void> struct HasOnUnhandledEventMethod : std::false_type {};
template <typename MachineType>
struct HasOnUnhandledEventMethod<MachineType, std::void_t<decltype(std::declval<MachineType&>().OnUnhandledEvent(std::declval<const EventBase&>()))>> : std::true_type {};
template <typename MachineType>
inline void InvokeOnUnhandledEventIfPresent(MachineType& machine, const EventBase& event) {
    if constexpr (HasOnUnhandledEventMethod<MachineType>::value) machine.OnUnhandledEvent(event);
}
template <class ContextList, class OutermostContext> struct Constructor;
template <typename Head, typename... Tail, class OutermostContext>
struct Constructor<mp::List<Head, Tail...>, OutermostContext> {
    template <typename ContextPtr>
    static void Construct(const ContextPtr& p, OutermostContext& out) {
        if constexpr (sizeof...(Tail) == 0) Head::DeepConstruct(p, out);
        else Constructor<mp::List<Tail...>, OutermostContext>::Construct(Head::ShallowConstruct(p, out), out);
    }
};
template <class CurrentContext, class DestState>
struct MakeContextList {
    using type = typename mp::PushBack<typename mp::BuildPath<typename DestState::ContextTypeList, CurrentContext>::type, DestState>::type;
};
} // namespace detail
template <typename Derived>
class Event : public detail::EventBase {
public:
    Event() noexcept : detail::EventBase(StaticTypeId()) {}
    const char* Name() const noexcept override {
        static const std::string name{detail::PrettyTypeName<Derived>()};
        return name.c_str();
    }
    detail::EventBase::Ptr Clone() const override { return detail::EventBase::Ptr(new Derived(static_cast<const Derived&>(*this))); }
private:
    template <class> friend class ::ufsm::Reaction;
    template <class, class, class> friend class ::ufsm::Transition;
    template <class> friend class ::ufsm::Deferral;
    static const void* StaticTypeId() noexcept { static const int tag = 0; return &tag; }
};
template <class EventType>
class Reaction {
public:
    template <typename StateType, typename EventBaseType>
    static Result React(StateType& state, const EventBaseType& event) {
        if (event.TypeId() == EventType::StaticTypeId()) {
            using ReturnT = decltype(state.React(static_cast<const EventType&>(event)));
            if constexpr (std::is_void_v<ReturnT>) { state.React(static_cast<const EventType&>(event)); return Result::kConsumed; }
            else return state.React(static_cast<const EventType&>(event));
        }
        return Result::kNoReaction;
    }
};
template <class EventType, class DestState, class Action = std::nullptr_t>
class Transition {
public:
    template <typename StateType, typename EventBaseType>
    static Result React(StateType& state, const EventBaseType& event) {
        if (event.TypeId() == EventType::StaticTypeId()) {
            if constexpr (std::is_same_v<Action, std::nullptr_t>) return state.template Transit<DestState>();
            else return state.template Transit<DestState>(Action{});
        }
        return Result::kNoReaction;
    }
};
template <class EventType, class DestState, class Action = std::nullptr_t> using transition = Transition<EventType, DestState, Action>;
template <class EventType>
class Deferral {
public:
    template <typename StateType, typename EventBaseType>
    static Result React(StateType& state, const EventBaseType& event) {
        if constexpr (std::is_same_v<EventType, detail::EventBase>) {
            return state.defer_event();
        } else {
            if (event.TypeId() == EventType::StaticTypeId()) return state.defer_event();
        }
        return Result::kNoReaction;
    }
};
template <class EventType> using deferral = Deferral<EventType>;
template <typename Derived, typename ContextState, typename InnerInitial = void>
class State : public detail::StateBase {
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
    const OutermostContextType& OutermostContext() const { return p_context_->OutermostContext(); }
    OutermostContextType& OutermostContext() { return const_cast<OutermostContextType&>(std::as_const(*this).OutermostContext()); }
    const OutermostContextBaseType& OutermostContextBase() const { return p_context_->OutermostContextBase(); }
    OutermostContextBaseType& OutermostContextBase() { return p_context_->OutermostContextBase(); }
    template <class Ev> void PostEvent(Ev&& event) { OutermostContext().PostEvent(std::forward<Ev>(event)); }
    [[nodiscard]] Result defer_event() {
        UFSM_ASSERT(p_context_->OutermostContextBase().current_event_ != nullptr);
        deferred_flag_ = true;
        return Result::kDeferEvent;
    }
    template <class TargetContext> const TargetContext& Context() const {
        if constexpr (std::is_same_v<TargetContext, Derived> || std::is_base_of_v<TargetContext, Derived>) {
            return *static_cast<const Derived*>(this);
        } else {
            return p_context_->template Context<TargetContext>();
        }
    }
    template <class TargetContext> TargetContext& Context() { return const_cast<TargetContext&>(std::as_const(*this).template Context<TargetContext>()); }
    template <class DestState, class Action = std::nullptr_t> [[nodiscard]] Result Transit(Action&& action = nullptr) { return TransitImpl<DestState>(std::forward<Action>(action)); }
    [[nodiscard]] constexpr Result forward_event() const noexcept { return Result::kForwardEvent; }
    [[nodiscard]] constexpr Result discard_event() const noexcept { return Result::kDiscardEvent; }
    [[nodiscard]] constexpr Result consume_event() const noexcept { return Result::kConsumed; }
    const void* TypeId() const noexcept override { return StaticTypeId(); }
    const char* Name() const noexcept override {
        static const std::string name{detail::PrettyTypeName<Derived>()};
        return name.c_str();
    }
    static const void* StaticTypeId() noexcept { static const int tag = 0; return &tag; }
protected:
    State(ContextType* context = nullptr) : p_context_(context) {}
    ~State() {
        if (p_context_ && deferred_flag_) {
            auto& out = p_context_->OutermostContextBase();
            for (; !out.deferred_events_.empty(); out.deferred_events_.pop_back())
                out.posted_events_.push_front(std::move(out.deferred_events_.back()));
        }
    }
private:
    template <class DestState, class Action>
    [[nodiscard]] Result TransitImpl(Action&& action) {
        using CommonCtx = typename mp::FindFirstCommon<ContextTypeList, typename DestState::ContextTypeList>::type;
        using TermState = typename mp::FindChildOf<typename mp::PushFront<ContextTypeList, Derived>::type, CommonCtx>::type;
        auto& term_state = Context<TermState>();
        auto& common_ctx = term_state.template Context<CommonCtx>();
        auto& out_base = common_ctx.OutermostContextBase();
#if !defined(NDEBUG)
        UFSM_ASSERT(!out_base.ufsm_in_transition_);
        struct UfsmTransitionGuard { bool& f; ~UfsmTransitionGuard() { f = false; } } guard{out_base.ufsm_in_transition_ = true};
#endif
        out_base.TerminateImpl(term_state);
        if constexpr (!std::is_same_v<std::remove_reference_t<Action>, std::nullptr_t>) {
            if constexpr (std::is_invocable_v<Action&&, CommonCtx&>) std::forward<Action>(action)(common_ctx);
            else std::forward<Action>(action)();
        }
        detail::Constructor<typename detail::MakeContextList<CommonCtx, DestState>::type, OutermostContextBaseType>::Construct(&common_ctx, out_base);
        return Result::kConsumed;
    }
    template <typename... Reactions>
    Result LocalReact(const detail::EventBase& event, mp::List<Reactions...>) {
        Result res = Result::kNoReaction;
        (void)((res = Reactions::React(*static_cast<Derived*>(this), event), res != Result::kNoReaction) || ...);
        return res == Result::kNoReaction ? Result::kForwardEvent : res;
    }
    Result ReactImpl(const detail::EventBase& event) override {
        auto res = LocalReact(event, typename Derived::reactions{});
        return res == Result::kForwardEvent ? p_context_->ReactImpl(event) : res;
    }
    template <typename, typename, typename> friend class ufsm::State;
    template <class, class> friend class ufsm::StateMachine;
    template <class, class> friend struct ufsm::detail::Constructor;
    static void DeepConstruct(const ContextPtrType& p_context, OutermostContextBaseType& out_context) {
        auto* p_inner = ShallowConstruct(p_context, out_context);
        if constexpr (!std::is_same_v<void, InnerInitial>) InnerInitial::DeepConstruct(p_inner, out_context);
    }
    static InnerContextPtrType ShallowConstruct(const ContextPtrType& p_context, OutermostContextBaseType& out_context) {
        std::unique_ptr<Derived> p;
        if constexpr (std::is_constructible_v<Derived, ContextType&>) {
            p = std::make_unique<Derived>(*p_context);
        } else {
            p = std::make_unique<Derived>();
            p->p_context_ = p_context;
        }
        return out_context.Add(std::move(p));
    }
    ContextPtrType p_context_;
    bool deferred_flag_ = false;
};
template <typename Derived, typename InnerInitial>
class StateMachine {
public:
    using InnerContextType = Derived;
    using OutermostContextType = Derived;
    using OutermostContextBaseType = StateMachine;
    using InnerContextPtrType = StateMachine*;
    using ContextTypeList = mp::List<>;
    using ContextType = void;

    void Initiate() {
        TerminateImpl();
        InnerInitial::DeepConstruct(static_cast<Derived*>(this), *static_cast<Derived*>(this));
    }
    void Terminate() {
#if !defined(NDEBUG)
        UFSM_ASSERT(!ufsm_in_transition_);
#endif
        TerminateImpl();
    }
    bool Terminated() const noexcept { return !current_state_; }
    template <class Ev> Result ProcessEvent(const Ev& event) { return ProcessEventLoop(event); }
    template <class Ev> void PostEvent(Ev&& event) { posted_events_.push_back(static_cast<const detail::EventBase&>(event).Clone()); }
    template <class StateT> bool IsInState() const {
        static_assert(std::is_base_of_v<detail::StateBase, StateT>, "StateT must be a state");
        for (const auto& p : active_path_) if (p && p->TypeId() == StateT::StaticTypeId()) return true;
        return false;
    }
    template <class TargetContext = Derived> const TargetContext& Context() const { return *static_cast<const Derived*>(this); }
    template <class TargetContext = Derived> TargetContext& Context() { return *static_cast<Derived*>(this); }
    const Derived& OutermostContext() const { return *static_cast<const Derived*>(this); }
    Derived& OutermostContext() { return *static_cast<Derived*>(this); }
    const StateMachine& OutermostContextBase() const { return *this; }
    StateMachine& OutermostContextBase() { return *this; }
protected:
    StateMachine() = default;
    virtual ~StateMachine() { TerminateImpl(); }
private:
    template <typename, typename, typename> friend class ufsm::State;
    template <class T> struct RestoreOnExit { T& slot; T prev; RestoreOnExit(T& s, T v) : slot(s), prev(std::exchange(s, v)) {} ~RestoreOnExit() { slot = prev; } };
#if !defined(NDEBUG)
    bool ufsm_in_transition_ = false;
#endif
    Result ReactImpl(const detail::EventBase&) { return Result::kForwardEvent; }
    template <class StateType> StateType* Add(std::unique_ptr<StateType> p_state) {
        p_state->SetActiveIndex(active_path_.size());
        StateType* raw = p_state.get();
        active_path_.push_back(std::move(p_state));
        if constexpr (std::is_same_v<typename StateType::InnerInitialType, void>) current_state_ = raw;
        return raw;
    }
    Result ProcessEventLoop(const detail::EventBase& event) {
#if !defined(NDEBUG)
        UFSM_ASSERT(!ufsm_in_transition_);
#endif
        if (ufsm_in_event_loop_) return ProcessEventImpl(event);
        RestoreOnExit<bool> guard(ufsm_in_event_loop_, true);
        Result last = ProcessEventImpl(event);
        for (; !posted_events_.empty(); posted_events_.pop_front())
            last = ProcessEventImpl(*posted_events_.front());
        return last;
    }
    Result ProcessEventImpl(const detail::EventBase& event) {
        RestoreOnExit<const detail::EventBase*> guard(current_event_, &event);
        auto res = current_state_ ? current_state_->ReactImpl(event) : Result::kForwardEvent;
        if (res == Result::kDeferEvent) { deferred_events_.push_back(event.Clone()); return Result::kConsumed; }
        if (res == Result::kForwardEvent) detail::InvokeOnUnhandledEventIfPresent(*static_cast<Derived*>(this), event);
        return res;
    }
    void ResetToDepth(std::size_t n) {
        while (active_path_.size() > n) active_path_.pop_back();
        current_state_ = active_path_.empty() ? nullptr : active_path_.back().get();
    }
    void TerminateImpl() { ResetToDepth(0); posted_events_.clear(); deferred_events_.clear(); }
    void TerminateImpl(detail::StateBase& state) {
        if (active_path_.empty()) { current_state_ = nullptr; return; }
        auto idx = state.ActiveIndex();
        UFSM_ASSERT(idx < active_path_.size() && active_path_[idx].get() == &state);
        ResetToDepth(idx < active_path_.size() && active_path_[idx].get() == &state ? idx : 0);
    }
    detail::StateBase* current_state_ = nullptr;
    std::vector<std::unique_ptr<detail::StateBase>> active_path_;
    const detail::EventBase* current_event_ = nullptr;
    std::deque<detail::EventBase::Ptr> deferred_events_, posted_events_;
    bool ufsm_in_event_loop_ = false;
};
} // namespace ufsm
#define FSM_STATE_MACHINE(N, I) struct I; struct N : ufsm::StateMachine<N, I>
#define _FSM_STATE_2(N, C) struct C; struct N : ufsm::State<N, C>
#define _FSM_STATE_3(N, C, I) struct C; struct I; struct N : ufsm::State<N, C, I>
#define FSM_STATE(...) _FSM_GET_MACRO(__VA_ARGS__, _FSM_STATE_3, _FSM_STATE_2)(__VA_ARGS__)
#define _FSM_GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define FSM_EVENT(N) struct N : ufsm::Event<N>
