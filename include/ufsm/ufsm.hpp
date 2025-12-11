#pragma once

#include <memory>
#include <type_traits>
#include <cassert>

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

namespace detail {

enum ReactionResult {
    no_reaction,
    do_forward_event,
    do_discard_event,
    consumed
};

class EventBase {
protected:
    virtual ~EventBase() = default;
};

class StateBase : public std::enable_shared_from_this<StateBase> {
public:
    using Ptr = std::shared_ptr<StateBase>;

    virtual ReactionResult ReactImpl(const EventBase& event) = 0;
    virtual void ExitImpl(Ptr& p_self, Ptr& p_outermost_unstable_state, bool perform_full_exit) = 0;

    void AddInnerState(StateBase* p_state) {
        p_inner_state_ = p_state;
    }

    void RemoveInnerState() {
        p_inner_state_ = nullptr;
    }

    void ExitCurrentState(Ptr& p_outermost_unstable_state, bool perform_full_exit) {
        if (p_inner_state_) {
            p_inner_state_->ExitCurrentState(p_outermost_unstable_state, perform_full_exit);
        } else {
            Ptr p_self = p_outermost_unstable_state;
            ExitImpl(p_self, p_outermost_unstable_state, perform_full_exit);
        }
    }

protected:
    StateBase() : p_inner_state_(nullptr) {}
    virtual ~StateBase() = default;

private:
    StateBase* p_inner_state_;
};

} // namespace detail

using Result = detail::ReactionResult;

template <typename Derived>
class Event : public detail::EventBase {};

template <class EventType>
class Reaction {
public:
    template <typename StateType, typename EventBaseType>
    static detail::ReactionResult React(StateType& state, const EventBaseType& event) {
        if (const auto* casted_event = dynamic_cast<const EventType*>(&event)) {
            if constexpr (std::is_void_v<decltype(state.React(*casted_event))>) {
                state.React(*casted_event);
                return detail::consumed;
            } else {
                return state.React(*casted_event);
            }
        }
        return detail::no_reaction;
    }
};

namespace detail {

template <class ContextList, class OutermostContext>
struct Constructor;

template <typename Head, typename... Tail, class OutermostContext>
struct Constructor<mp::List<Head, Tail...>, OutermostContext> {
    template <typename ContextPtr>
    static void Construct(const ContextPtr& p_context, OutermostContext& out_context) {
        if constexpr (sizeof...(Tail) == 0) {
            Head::DeepConstruct(p_context, out_context);
        } else {
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

template <typename Derived, typename ContextState, typename InnerInitial = void>
class State : public detail::StateBase {
public:
    using InnerInitialType = InnerInitial;
    using reactions = mp::List<>;
    using ContextType = typename ContextState::InnerContextType;
    using ContextPtrType = typename ContextType::InnerContextPtrType;
    using OutermostContextType = typename ContextType::OutermostContextType;
    using InnerContextType = Derived;
    using InnerContextPtrType = std::shared_ptr<Derived>;
    using OutermostContextBaseType = typename ContextType::OutermostContextBaseType;
    using ContextTypeList = typename mp::PushFront<typename ContextType::ContextTypeList, ContextType>::type;

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

    template <class DestState>
    Result Transit() {
        return TransitImpl<DestState>([](auto&) {});
    }

    static void InitialDeepConstruct(OutermostContextBaseType& out_context) {
        DeepConstruct(&out_context, out_context);
    }

    static void DeepConstruct(const ContextPtrType& p_context, OutermostContextBaseType& out_context) {
        DeepConstructInner(ShallowConstruct(p_context, out_context), out_context);
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

    static void DeepConstructInner(const InnerContextPtrType& p_inner, OutermostContextBaseType& out_context) {
        if constexpr (!std::is_same_v<void, InnerInitial>) {
            InnerInitial::DeepConstruct(p_inner, out_context);
        }
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

    void ExitImpl(InnerContextPtrType& p_self, Ptr& p_outer_unstable, bool full_exit) {
        long use_count = weak_from_this().use_count();
        if (use_count == 2 && p_outer_unstable.get() == this) {
            p_context_->SetOutermostUnstableState(p_outer_unstable);
        } else if (use_count == 2) {
            return;
        }
        if (use_count <= 2) {
            if (!p_outer_unstable) {
                p_context_->SetOutermostUnstableState(p_outer_unstable);
            }
            ContextPtrType p_ctx = p_context_;
            p_self = nullptr;
            p_ctx->ExitImpl(p_ctx, p_outer_unstable, full_exit);
        }
    }

    detail::ReactionResult ReactImpl(const detail::EventBase& event) override {
        auto res = LocalReact(event, typename Derived::reactions{});
        if (res == detail::do_forward_event) {
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
        p_context_->AddInnerState(this);
    }

    ~State() {
        if (p_context_) {
            p_context_->RemoveInnerState();
        }
    }

    void SetContext(const ContextPtrType& p_context) {
        p_context_ = p_context;
        p_context_->AddInnerState(this);
    }

    void ExitImpl(Ptr& p_self, Ptr& p_outer_unstable, bool full_exit) override {
        InnerContextPtrType p_derived = std::static_pointer_cast<Derived>(shared_from_this());
        p_self = nullptr;
        ExitImpl(p_derived, p_outer_unstable, full_exit);
    }

    template <class DestState, class Action>
    Result TransitImpl(const Action& action) {
        using CommonCtx = typename mp::FindFirstCommon<ContextTypeList, typename DestState::ContextTypeList>::type;
        using TermState = typename mp::FindChildOf<typename mp::PushFront<ContextTypeList, Derived>::type, CommonCtx>::type;

        TermState& term_state(Context<TermState>());
        const auto p_common = term_state.template ContextPtr<CommonCtx>();
        auto& out_base = p_common->OutermostContextBase();

        out_base.TerminateAsPartOfTransit(term_state);
        action(*p_common);

        detail::Constructor<typename detail::MakeContextList<CommonCtx, DestState>::type, OutermostContextBaseType>::Construct(p_common, out_base);
        return detail::consumed;
    }

    template <typename... Reactions>
    detail::ReactionResult LocalReact(const detail::EventBase& event, mp::List<Reactions...>) {
        detail::ReactionResult res = detail::no_reaction;
        (void)((res = Reactions::React(*static_cast<Derived*>(this), event), res != detail::no_reaction) || ...);
        return res == detail::no_reaction ? detail::do_forward_event : res;
    }

private:
    ContextPtrType p_context_;
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
        Terminate();
        Terminator terminator_guard(*this);
        InnerInitial::InitialDeepConstruct(*static_cast<Derived*>(this));
        terminator_guard.dismiss();
    }

    void Terminate() {
        Terminator terminator_guard(*this);
        TerminateImpl(true);
        terminator_guard.dismiss();
    }

    bool Terminated() const {
        return !p_outermost_state_;
    }

    void ProcessEvent(const detail::EventBase& event) {
        SendEvent(event);
    }

    void ExitImpl(InnerContextPtrType&, detail::StateBase::Ptr&, bool) {}

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
        TerminateImpl(state, true);
        is_innermost_common_outer_ = true;
    }

    detail::ReactionResult ReactImpl(const detail::EventBase&) {
        return detail::do_forward_event;
    }

    template <class StateType, class ParentType>
    void Add(const std::shared_ptr<StateType>& p_state, const ParentType& parent) {
        detail::StateBase::Ptr p_candidate;
        if constexpr (std::is_same_v<typename StateType::InnerInitialType, void>) {
            current_state_ = p_state;
            p_candidate = nullptr;
        } else {
            p_candidate = p_state;
        }

        bool is_parent_unstable = false;
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

    void AddInnerState(detail::StateBase* p_state) {
        p_outermost_state_ = p_state;
    }

    void RemoveInnerState() {
        p_outermost_state_ = nullptr;
    }

    void UnconsumedEvent(const detail::EventBase&) {}

protected:
    StateMachine()
        : p_outermost_state_(nullptr),
          is_innermost_common_outer_(false),
          p_outermost_unstable_state_(nullptr) {}

    virtual ~StateMachine() {
        TerminateImpl(false);
    }

private:
    class Terminator {
        StateMachine& machine_;
        bool dismissed_;

    public:
        Terminator(StateMachine& machine) : machine_(machine), dismissed_(false) {}
        ~Terminator() {
            if (!dismissed_) machine_.TerminateImpl(false);
        }
        void dismiss() {
            dismissed_ = true;
        }
    };

    detail::ReactionResult SendEvent(const detail::EventBase& event) {
        Terminator terminator_guard(*this);
        detail::ReactionResult res = detail::do_forward_event;
        if (current_state_) {
            res = current_state_->ReactImpl(event);
        }
        terminator_guard.dismiss();
        if (res == detail::do_forward_event) {
            static_cast<Derived*>(this)->UnconsumedEvent(event);
        }
        return res;
    }

    void TerminateImpl(bool full) {
        if (!Terminated()) {
            TerminateImpl(*p_outermost_state_, full);
        }
    }

    void TerminateImpl(detail::StateBase& state, bool full) {
        is_innermost_common_outer_ = false;
        if (p_outermost_unstable_state_) {
            state.ExitCurrentState(p_outermost_unstable_state_, full);
        } else if (current_state_) {
            auto p = current_state_;
            current_state_ = nullptr;
            p->ExitImpl(p, p_outermost_unstable_state_, full);
        } else {
            state.ExitCurrentState(p_outermost_unstable_state_, full);
        }
    }

    detail::StateBase::Ptr current_state_;
    detail::StateBase* p_outermost_state_;
    bool is_innermost_common_outer_;
    detail::StateBase::Ptr p_outermost_unstable_state_;
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
