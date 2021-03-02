#pragma once

#include <ufsm/detail/leaf_state.h>
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
  using type =
      typename type::If<type::IsList<T>::value, T, type::List<T>>::Type;
};

struct NoInnerInitial {};

template <typename MostDerived, typename Context, typename InnerInitial>
struct StateBaseType {
 public:
  using type = typename type::If<
      std::is_same<detail::NoInnerInitial, InnerInitial>::value,
      typename RttiPolicy::RttiDerivedType<MostDerived, LeafState>,
      typename RttiPolicy::RttiDerivedType<MostDerived, NodeState<1>>>::Type;
};

}  // namespace detail

template <typename MostDerived, typename Context,
          typename InnerInitial = detail::NoInnerInitial>
class State
    : public detail::StateBaseType<
          MostDerived, typename Context::InnerContextType, InnerInitial>::type {
  using BaseType = typename detail::StateBaseType<
      MostDerived, typename Context::InnerContextType, InnerInitial>::type;

 public:
  using reactions = type::List<>;
  using ContextType = typename Context::InnerContextType;
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
  using OrthogonalPosition = typename Context::InnerOrthogonalPosition;

  typedef MostDerived InnerContextType;
  typedef std::integral_constant<detail::OrthogonalPositionType, 0>
      InnerOrthogonalPosition;
  typedef std::shared_ptr<InnerContextType> InnerContextPtrType;

  using OutermostContextBaseType =
      typename ContextType::OutermostContextBaseType;

  static void InitialDeepConstruct(
      OutermostContextBaseType& outermost_context_base) {
    DeepConstruct(&outermost_context_base, outermost_context_base);
  }

  static void DeepConstruct(const ContextPtrType& pContext,
                            OutermostContextBaseType& outermostContextBase) {
    const InnerContextPtrType pInnerContext(
        ShallowConstruct(pContext, outermostContextBase));
    DeepConstructInner(pInnerContext, outermostContextBase);
  }

  static InnerContextPtrType ShallowConstruct(
      const ContextPtrType& pContext,
      OutermostContextBaseType& outermostContextBase) {
    const InnerContextPtrType p_inner_context(new MostDerived);
    p_inner_context->SetContext(pContext);
    outermostContextBase.Add(p_inner_context);
    return p_inner_context;
  }

  void SetContext(const ContextPtrType& pContext) {
    assert(pContext != nullptr);
    p_context_ = pContext;
    BaseType::SetContext(OrthogonalPosition::value, pContext);
  }

  static void DeepConstructInner(
      const InnerContextPtrType& p_inner_context,
      OutermostContextBaseType& outermost_context_base) {
    typedef typename type::If<
        std::is_same<detail::NoInnerInitial, InnerInitial>::value,
        DeepConstructInnerImplEmpty, DeepConstructInnerImplNonEmpty>::Type impl;
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
        const InnerContextPtrType& pInnerContext,
        OutermostContextBaseType& outermostContextBase) {
      Inner::DeepConstruct(pInnerContext, outermostContextBase);
    }
  };

  struct DeepConstructInnerImplEmpty {
    template <typename Inner>
    static void DeepConstructInnerImpl(const InnerContextPtrType&,
                                       OutermostContextBaseType&) {}
  };

  virtual void ExitImpl(
      typename BaseType::DirectStateBasePtrType& pSelf,
      typename detail::StateBase::NodeStateBasePtrType& pOutermostUnstableState,
      bool performFullExit) {
    //    InnerContextPtrType pMostDerivedSelf =
    //        polymorphic_downcast<MostDerived*>(this->get());
    //    pSelf = 0;
    //    exit_impl(pMostDerivedSelf, pOutermostUnstableState, performFullExit);
    //    // TODO:
  }

  struct LocalReactImplNonEmpty {
    template <typename ReactionList, typename State>
    static detail::ReactionResult LocalReactImpl(State& stt,
                                                 const detail::EventBase& evt) {
      detail::ReactionResult reaction_result =
          type::Front<ReactionList>::Type::React(
              *polymorphic_cast<MostDerived*>(&stt), evt);

      if (reaction_result == detail::no_reaction) {
        reaction_result = stt.template LocalReact<
            typename type::PopFront<ReactionList>::Type>(evt);
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
        typename type::If<type::Empty<ReactionList>::value, LocalReactImplEmpty,
                          LocalReactImplNonEmpty>::Type;
    return impl::template LocalReactImpl<ReactionList>(*this, evt);
  }

  virtual const detail::StateBase* OuterStatePtr() const {
    typedef typename type::If<
        std::is_same<OutermostContextType, ContextType>::value,
        OuterStatePtrImplOutermost, OuterStatePtrImplNonOutermost>::Type impl;
    return impl::OuterStatePtrImpl(*this);
  }

  detail::ReactionResult ReactImpl(const detail::EventBase& evt) override {
    using reaction_list = typename MostDerived::reactions;
    detail::ReactionResult reaction_result = LocalReact<reaction_list>(evt);

    // At this point we can only safely access pContext_ if the handler did
    // not return do_discard_event!
    if (reaction_result == detail::do_forward_event) {
      // TODO: forward event
      //      reaction_result = pContext_->ReactImpl(evt, eventType);
    }

    return reaction_result;
  }

 private:
  ContextPtrType p_context_;
};

}  // namespace ufsm
