#pragma once

#include <ufsm/detail/constructor.h>
#include <ufsm/detail/leaf_state.h>
#include <ufsm/detail/node_state.h>
#include <ufsm/detail/state_base.h>
#include <ufsm/reaction.h>
#include <ufsm/result.h>
#include <ufsm/type/list.h>

#include <type_traits>

namespace ufsm {

namespace detail {

struct NoInnerInitial {};

template <typename MostDerived, typename InnerInitial>
using StateBaseType = mp::If<std::is_same<NoInnerInitial, InnerInitial>::value,
                             typename RttiPolicy::RttiDerivedType<MostDerived, LeafState>,
                             typename RttiPolicy::RttiDerivedType<MostDerived, NodeState<1>>>;

}  // namespace detail

template <typename MostDerived, typename Context_, typename InnerInitial = detail::NoInnerInitial>
class State : public detail::StateBaseType<MostDerived, InnerInitial> {
  using BaseType = detail::StateBaseType<MostDerived, InnerInitial>;

 public:
  using reactions = mp::List<>;
  using ContextType = typename Context_::InnerContextType;
  using ContextPtrType = typename ContextType::InnerContextPtrType;

  using OutermostContextType = typename ContextType::OutermostContextType;

  OutermostContextType& OutermostContext() {
    assert(p_context_.get() != nullptr);
    return p_context_->OutermostContext();
  }

  const OutermostContextType& OutermostContext() const {
    assert(p_context_.get() != nullptr);
    return p_context_->OutermostContext();
  }

  template <class OtherContext>
  OtherContext& Context() {
    if constexpr (std::is_base_of<OtherContext, MostDerived>::value) {
      return *static_cast<MostDerived*>(this);
    } else {
      assert(p_context_ != nullptr);
      return p_context_->template Context<OtherContext>();
    }
  }

  template <class OtherContext>
  const OtherContext& Context() const {
    if constexpr (std::is_base_of<OtherContext, MostDerived>::value) {
      return *static_cast<const MostDerived*>(this);
    } else {
      assert(p_context_ != nullptr);
      return p_context_->template Context<OtherContext>();
    }
  }

  template <class DestinationState>
  Result Transit() {
    return TransitImpl<DestinationState, OutermostContextType>([](auto&) {});
  }

 protected:
  State() : p_context_(nullptr) {}

  virtual ~State() {
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

  using InnerContextType = MostDerived;
  using InnerOrthogonalPosition = std::integral_constant<detail::OrthogonalPositionType, 0>;
  using InnerContextPtrType = std::shared_ptr<InnerContextType>;
  using OutermostContextBaseType = typename ContextType::OutermostContextBaseType;
  using ContextTypeList = typename mp::PushFront<typename ContextType::ContextTypeList, ContextType>::type;

  template <class OtherContext>
  const typename OtherContext::InnerContextPtrType& ContextPtr() const {
    assert(p_context_ != nullptr);
    if constexpr (std::is_same<OtherContext, ContextType>::value) {
      return p_context_;
    } else {
      return p_context_->template ContextPtr<OtherContext>();
    }
  }

  static void InitialDeepConstruct(OutermostContextBaseType& outermost_context_base) {
    DeepConstruct(&outermost_context_base, outermost_context_base);
  }

  static void DeepConstruct(const ContextPtrType& p_context, OutermostContextBaseType& outermost_context_base) {
    const InnerContextPtrType p_inner_context(ShallowConstruct(p_context, outermost_context_base));
    DeepConstructInner(p_inner_context, outermost_context_base);
  }

  static InnerContextPtrType ShallowConstruct(const ContextPtrType& pContext,
                                              OutermostContextBaseType& outermostContextBase) {
    const InnerContextPtrType p_inner_context = std::make_shared<MostDerived>();
    p_inner_context->SetContext(pContext);
    outermostContextBase.Add(p_inner_context);
    return p_inner_context;
  }

  void SetContext(const ContextPtrType& p_context) {
    assert(p_context != nullptr);
    p_context_ = p_context;
    BaseType::SetContext(OrthogonalPosition::value, p_context);
  }

  static void DeepConstructInner(const InnerContextPtrType& p_inner_context,
                                 OutermostContextBaseType& outermost_context_base) {
    if constexpr (!std::is_same<detail::NoInnerInitial, InnerInitial>::value) {
      InnerInitial::DeepConstruct(p_inner_context, outermost_context_base);
    }
  }

  virtual void ExitImpl(typename BaseType::DirectStateBasePtrType& p_self,
                        typename detail::StateBase::NodeStateBasePtrType& p_outermost_unstable_state,
                        bool perform_full_exit) override {
    InnerContextPtrType p_most_derived_self = std::static_pointer_cast<MostDerived>(BaseType::get_shared_ptr());
    p_self = nullptr;
    ExitImpl(p_most_derived_self, p_outermost_unstable_state, perform_full_exit);
  }

  void ExitImpl(InnerContextPtrType& p_self,
                typename detail::StateBase::NodeStateBasePtrType& p_outermost_unstable_state, bool perform_full_exit) {
    switch (this->get_weak_ptr().use_count()) {
      case 2:
        if (p_outermost_unstable_state.get() == static_cast<detail::StateBase*>(this)) {
          p_context_->SetOutermostUnstableState(p_outermost_unstable_state);
          [[fallthrough]];
        } else {
          break;
        }
      case 1: {
        if (p_outermost_unstable_state == nullptr) {
          p_context_->SetOutermostUnstableState(p_outermost_unstable_state);
        }

        ContextPtrType p_context = p_context_;
        p_self = nullptr;
        p_context->ExitImpl(p_context, p_outermost_unstable_state, perform_full_exit);
        break;
      }
      default:
        break;
    }
  }

  void SetOutermostUnstableState(typename detail::StateBase::NodeStateBasePtrType& p_outermost_unstable_state) {
    p_outermost_unstable_state = std::static_pointer_cast<detail::NodeStateBase>(BaseType::get_shared_ptr());
  }

  template <class DestinationState, class TransitionContext, class TransitionAction>
  Result TransitImpl(const TransitionAction& transition_action) {
    constexpr int termination_state_position =
        mp::FindIf<ContextTypeList,
                   mp::Contains<typename DestinationState::ContextTypeList, mp::placeholders::_>>::value;
    using CommonContextType = typename mp::At<ContextTypeList, termination_state_position>::type;
    using PossibleTransitionContexts = typename mp::PushFront<ContextTypeList, MostDerived>::type;
    using TerminationStateType = typename mp::At<PossibleTransitionContexts, termination_state_position>::type;

    TerminationStateType& termination_state(Context<TerminationStateType>());
    const typename CommonContextType::InnerContextPtrType p_common_context(
        termination_state.template ContextPtr<CommonContextType>());
    OutermostContextBaseType& outermost_context_base(p_common_context->OutermostContextBase());

    outermost_context_base.TerminateAsPartOfTransit(termination_state);
    transition_action(*p_common_context);

    using ContextListType = typename detail::MakeContextList<CommonContextType, DestinationState>::type;

    detail::Constructor<ContextListType, OutermostContextBaseType>::Construct(p_common_context, outermost_context_base);

    return detail::do_discard_event;
  }

  template <typename... Reactions>
  detail::ReactionResult LocalReact(const detail::EventBase& evt, mp::List<Reactions...>) {
    detail::ReactionResult result = detail::no_reaction;
    // Fold expression to find the first reaction that handles the event
    [[maybe_unused]] bool _ =
        ((result = Reactions::React(*static_cast<MostDerived*>(this), evt), result != detail::no_reaction) || ...);
    return result == detail::no_reaction ? detail::do_forward_event : result;
  }

  OutermostContextBaseType& OutermostContextBase() {
    assert(p_context_ != nullptr);
    return p_context_->OutermostContextBase();
  }

  const OutermostContextBaseType& OutermostContextBase() const {
    assert(p_context_ != nullptr);
    return p_context_->OutermostContextBase();
  }

  virtual const detail::StateBase* OuterStatePtr() const override {
    if constexpr (std::is_same<OutermostContextType, ContextType>::value) {
      return nullptr;
    } else {
      return p_context_.get();
    }
  }

  detail::ReactionResult ReactImpl(const detail::EventBase& evt) override {
    using reaction_list = typename MostDerived::reactions;
    detail::ReactionResult reaction_result = LocalReact(evt, reaction_list{});

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
