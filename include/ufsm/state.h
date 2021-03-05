#pragma once

#include <ufsm/detail/constructor.h>
#include <ufsm/detail/leaf_state.h>
#include <ufsm/detail/meta_debug.h>
#include <ufsm/detail/node_state.h>
#include <ufsm/detail/polymorphic_cast.h>
#include <ufsm/detail/state_base.h>
#include <ufsm/reaction.h>
#include <ufsm/result.h>
#include <ufsm/type/list.h>

#include <iostream>
#include <type_traits>

namespace ufsm {

namespace detail {

template <typename T>
struct MakeList {
  using type = typename mp::If<mp::IsList<T>::value, T, mp::List<T>>::type;
};

struct NoInnerInitial {};

template <typename MostDerived, typename Context, typename InnerInitial>
struct StateBaseType {
 public:
  using type = typename mp::If<
      std::is_same<detail::NoInnerInitial, InnerInitial>::value,
      typename RttiPolicy::RttiDerivedType<MostDerived, LeafState>,
      typename RttiPolicy::RttiDerivedType<MostDerived, NodeState<1>>>::type;
};

struct NoTransitionFunction {
  template <typename CommonContext>
  void operator()(CommonContext&) const {}
};

}  // namespace detail

template <typename MostDerived, typename Context_,
          typename InnerInitial = detail::NoInnerInitial>
class State : public detail::StateBaseType<MostDerived,
                                           typename Context_::InnerContextType,
                                           InnerInitial>::type {
  using BaseType = typename detail::StateBaseType<
      MostDerived, typename Context_::InnerContextType, InnerInitial>::type;

 public:
  using reactions = mp::List<>;
  using ContextType = typename Context_::InnerContextType;
  using ContextPtrType = typename ContextType::InnerContextPtrType;

  using OutermostContextType = typename ContextType::OutermostContextType;

  OutermostContextType& OutermostContext() {
    // This assert fails when an attempt is made to access the state machine
    // from a constructor of a state that is *not* a subtype of state<>.
    // To correct this, derive from state<> instead of simple_state<>.
    assert(p_context_.get() != 0);
    return p_context_->OutermostContext();
  }

  const OutermostContextType& OutermostContext() const {
    // This assert fails when an attempt is made to access the state machine
    // from a constructor of a state that is *not* a subtype of state<>.
    // To correct this, derive from state<> instead of simple_state<>.
    assert(p_context_.get() != 0);
    return p_context_->OutermostContext();
  }

  template <class OtherContext>
  OtherContext& Context() {
    typedef
        typename mp::If<std::is_base_of<OtherContext, MostDerived>::value,
                        ContextImplThisContext, ContextImplOtherContext>::type
            impl;
    return impl::template ContextImpl<OtherContext>(*this);
  }

  template <class OtherContext>
  const OtherContext& Context() const {
    typedef
        typename mp::If<std::is_base_of<OtherContext, MostDerived>::value,
                        ContextImplThisContext, ContextImplOtherContext>::type
            impl;
    return impl::template ContextImpl<OtherContext>(*this);
  }

  template <class DestinationState>
  Result Transit() {
    return TransitImpl<DestinationState, OutermostContextType>(
        detail::NoTransitionFunction());
  }

 protected:
  State() : p_context_(nullptr) {}

  ~State() {
    // As a result of a throwing derived class constructor, this destructor
    // can be called before the context is set.
    if (p_context_ != nullptr) {
      if (this->deferred_events()) {
        //        OutermostContextBase().release_events(); // TODO:
      }

      p_context_->RemoveInnerState(OrthogonalPosition::value);
    }
  }

 public:
  using OrthogonalPosition = typename Context_::InnerOrthogonalPosition;

  typedef MostDerived InnerContextType;
  typedef std::integral_constant<detail::OrthogonalPositionType, 0>
      InnerOrthogonalPosition;
  typedef std::shared_ptr<InnerContextType> InnerContextPtrType;
  using OutermostContextBaseType =
      typename ContextType::OutermostContextBaseType;
  using ContextTypeList =
      typename mp::PushFront<typename ContextType::ContextTypeList,
                             ContextType>::type;

  template <class OtherContext>
  const typename OtherContext::InnerContextPtrType& ContextPtr() const {
    typedef typename mp::If<std::is_same<OtherContext, ContextType>::value,
                            ContextPtrImplMyContext,
                            ContextPtrImplOtherContext>::type impl;

    return impl::template ContextPtrImpl<OtherContext>(*this);
  }

  static void InitialDeepConstruct(
      OutermostContextBaseType& outermost_context_base) {
    DeepConstruct(&outermost_context_base, outermost_context_base);
  }

  static void DeepConstruct(const ContextPtrType& p_context,
                            OutermostContextBaseType& outermost_context_base) {
    const InnerContextPtrType p_inner_context(
        ShallowConstruct(p_context, outermost_context_base));
    DeepConstructInner(p_inner_context, outermost_context_base);
  }

  static InnerContextPtrType ShallowConstruct(
      const ContextPtrType& pContext,
      OutermostContextBaseType& outermostContextBase) {
    const InnerContextPtrType p_inner_context(new MostDerived);
    p_inner_context->SetContext(pContext);
    outermostContextBase.Add(p_inner_context);
    return p_inner_context;
  }

  void SetContext(const ContextPtrType& p_context) {
    assert(p_context != nullptr);
    p_context_ = p_context;
    BaseType::SetContext(OrthogonalPosition::value, p_context);
  }

  static void DeepConstructInner(
      const InnerContextPtrType& p_inner_context,
      OutermostContextBaseType& outermost_context_base) {
    typedef typename mp::If<
        std::is_same<detail::NoInnerInitial, InnerInitial>::value,
        DeepConstructInnerImplEmpty, DeepConstructInnerImplNonEmpty>::type impl;
    impl::template DeepConstructInnerImpl<InnerInitial>(p_inner_context,
                                                        outermost_context_base);
  }

  struct OuterStatePtrImplNonOutermost {
    template <class State>
    static const detail::StateBase* OuterStatePtrImpl(const State& stt) {
      return stt.p_context_.get();
    }
  };
  friend struct OuterStatePtrImplNonOutermost;

  struct OuterStatePtrImplOutermost {
    template <class State>
    static const detail::StateBase* OuterStatePtrImpl(const State&) {
      return nullptr;
    }
  };

  struct DeepConstructInnerImplNonEmpty {
    template <typename Inner>
    static void DeepConstructInnerImpl(
        const InnerContextPtrType& p_inner_context,
        OutermostContextBaseType& outermost_context_base) {
      Inner::DeepConstruct(p_inner_context, outermost_context_base);
    }
  };

  struct DeepConstructInnerImplEmpty {
    template <typename Inner>
    static void DeepConstructInnerImpl(const InnerContextPtrType&,
                                       OutermostContextBaseType&) {}
  };

  virtual void ExitImpl(typename BaseType::DirectStateBasePtrType& p_self,
                        typename detail::StateBase::NodeStateBasePtrType&
                            p_outermost_unstable_state,
                        bool perform_full_exit) {
    InnerContextPtrType p_most_derived_self =
        std::dynamic_pointer_cast<MostDerived>(BaseType::get_shared_ptr());
    p_self = 0;
    ExitImpl(p_most_derived_self, p_outermost_unstable_state,
             perform_full_exit);
  }

  void ExitImpl(InnerContextPtrType& p_self,
                typename detail::StateBase::NodeStateBasePtrType&
                    p_outermost_unstable_state,
                bool perform_full_exit) {
    switch (this->get_weak_ptr().use_count()) {
      case 2:
        if (p_outermost_unstable_state.get() ==
            static_cast<detail::StateBase*>(this)) {
          p_context_->SetOutermostUnstableState(p_outermost_unstable_state);
          // BOOST_FALLTHROUGH // goto case 1
        } else {
          break;
        }
      case 1: {
        if (p_outermost_unstable_state == nullptr) {
          p_context_->SetOutermostUnstableState(p_outermost_unstable_state);
        }

        if (perform_full_exit) {
          p_self->Exit();
        }

        ContextPtrType p_context = p_context_;
        p_self = 0;
        p_context->ExitImpl(p_context, p_outermost_unstable_state,
                            perform_full_exit);
        break;
      }
      default:
        break;
    }
  }

  void SetOutermostUnstableState(
      typename detail::StateBase::NodeStateBasePtrType&
          p_outermost_unstable_state) {
    p_outermost_unstable_state =
        std::dynamic_pointer_cast<detail::NodeStateBase>(
            BaseType::get_shared_ptr());
  }

  struct ContextPtrImplOtherContext {
    template <class OtherContext, class State>
    static const typename OtherContext::InnerContextPtrType& ContextPtrImpl(
        const State& stt) {
      // This assert fails when an attempt is made to access an outer
      // context from a constructor of a state that is *not* a subtype of
      // state<>. To correct this, derive from state<> instead of
      // simple_state<>.
      assert(stt.p_context_ != 0);
      return stt.p_context_->template ContextPtr<OtherContext>();
    }
  };
  friend struct ContextPtrImplOtherContext;

  struct ContextPtrImplMyContext {
    template <class OtherContext, class State>
    static const typename OtherContext::InnerContextPtrType& ContextPtrImpl(
        const State& stt) {
      // This assert fails when an attempt is made to access an outer
      // context from a constructor of a state that is *not* a subtype of
      // state<>. To correct this, derive from state<> instead of
      // simple_state<>.
      assert(stt.p_context_ != 0);
      return stt.p_context_;
    }
  };
  friend struct ContextPtrImplMyContext;

  struct ContextImplOtherContext {
    template <class OtherContext, class State>
    static OtherContext& ContextImpl(State& stt) {
      // This assert fails when an attempt is made to access an outer
      // context from a constructor of a state that is *not* a subtype of
      // state<>. To correct this, derive from state<> instead of
      // simple_state<>.
      assert(stt.p_context_ != 0);
      return stt.p_context_->template Context<OtherContext>();
    }
  };
  friend struct ContextImplOtherContext;

  struct ContextImplThisContext {
    template <class OtherContext, class State>
    static OtherContext& ContextImpl(State& stt) {
      return *polymorphic_downcast<MostDerived*>(&stt);
    }
  };
  friend struct ContextImplThisContext;

  template <class DestinationState, class TransitionContext,
            class TransitionAction>
  Result TransitImpl(const TransitionAction& transition_action) {
    constexpr int termination_state_position =
        mp::FindIf<ContextTypeList,
                   mp::Contains<typename DestinationState::ContextTypeList,
                                mp::placeholders::_>>::value;
    using CommonContextType =
        typename mp::At<ContextTypeList, termination_state_position>::type;
    using PossibleTransitionContexts =
        typename mp::PushFront<ContextTypeList, MostDerived>::type;
    using TerminationStateType =
        typename mp::At<PossibleTransitionContexts,
                        termination_state_position>::type;

    TerminationStateType& termination_state(Context<TerminationStateType>());
    const typename CommonContextType::InnerContextPtrType p_common_context(
        termination_state.template ContextPtr<CommonContextType>());
    OutermostContextBaseType& outermost_context_base(
        p_common_context->OutermostContextBase());

    outermost_context_base.TerminateAsPartOfTransit(termination_state);
    transition_action(*p_common_context);

    using ContextListType =
        typename detail::MakeContextList<CommonContextType,
                                         DestinationState>::type;

    detail::Constructor<ContextListType, OutermostContextBaseType>::Construct(
        p_common_context, outermost_context_base);

    return detail::do_discard_event;
  }

  struct LocalReactImplNonEmpty {
    template <typename ReactionList, typename State>
    static detail::ReactionResult LocalReactImpl(State& stt,
                                                 const detail::EventBase& evt) {
      detail::ReactionResult reaction_result =
          mp::Front<ReactionList>::type::React(
              *polymorphic_cast<MostDerived*>(&stt), evt);

      if (reaction_result == detail::no_reaction) {
        reaction_result =
            stt.template LocalReact<typename mp::PopFront<ReactionList>::type>(
                evt);
      }

      return reaction_result;
    }
  };
  friend struct LocalReactImplNonEmpty;

  struct LocalReactImplEmpty {
    template <typename ReactionList, typename State>
    static detail::ReactionResult LocalReactImpl(State&,
                                                 const detail::EventBase&) {
      return detail::do_forward_event;
    }
  };

  template <class ReactionList>
  detail::ReactionResult LocalReact(const detail::EventBase& evt) {
    using impl =
        typename mp::If<mp::Empty<ReactionList>::value, LocalReactImplEmpty,
                        LocalReactImplNonEmpty>::type;
    return impl::template LocalReactImpl<ReactionList>(*this, evt);
  }

  OutermostContextBaseType& OutermostContextBase() {
    // This assert fails when an attempt is made to access the state machine
    // from a constructor of a state that is *not* a subtype of state<>.
    // To correct this, derive from state<> instead of simple_state<>.
    assert(p_context_ != 0);
    return p_context_->OutermostContextBase();
  }

  const OutermostContextBaseType& OutermostContextBase() const {
    // This assert fails when an attempt is made to access the state machine
    // from a constructor of a state that is *not* a subtype of state<>.
    // To correct this, derive from state<> instead of simple_state<>.
    assert(p_context_ != 0);
    return p_context_->OutermostContextBase();
  }

  virtual const detail::StateBase* OuterStatePtr() const {
    typedef
        typename mp::If<std::is_same<OutermostContextType, ContextType>::value,
                        OuterStatePtrImplOutermost,
                        OuterStatePtrImplNonOutermost>::type impl;
    return impl::OuterStatePtrImpl(*this);
  }

  detail::ReactionResult ReactImpl(const detail::EventBase& evt) override {
    using reaction_list = typename MostDerived::reactions;
    detail::ReactionResult reaction_result = LocalReact<reaction_list>(evt);

    // At this point we can only safely access pContext_ if the handler did
    // not return do_discard_event!
    if (reaction_result == detail::do_forward_event) {
      reaction_result = p_context_->ReactImpl(evt);
    }

    return reaction_result;
  }

 private:
  ContextPtrType p_context_;
};

}  // namespace ufsm
