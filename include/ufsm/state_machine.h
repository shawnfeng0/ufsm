//
// Created by fs on 2/25/21.
//

#pragma once

#include <ufsm/detail/polymorphic_cast.h>
#include <ufsm/detail/state_base.h>
#include <ufsm/event_base.h>

namespace ufsm {

namespace detail {

class SendFunction {
 public:
  //////////////////////////////////////////////////////////////////////////
  SendFunction(StateBase& toState, const EventBase& evt)
      : to_state_(toState), evt_(evt) {}

  Result operator()() { return to_state_.ReactImpl(evt_); }

  SendFunction& operator=(const SendFunction&) = delete;

 private:
  StateBase& to_state_;
  const EventBase& evt_;
};

}  // namespace detail

template <typename MostDerived, typename InitialState>
class StateMachine {
 public:
  using EventBasePtrType = std::shared_ptr<const detail::EventBase>;

  using InnerContextType = MostDerived;

  using InnerOrthogonalPosition =
      std::integral_constant<detail::OrthogonalPositionType, 0>;
  using NoOfOrthogonalRegions =
      std::integral_constant<detail::OrthogonalPositionType, 1>;

  using OutermostContextType = MostDerived;
  using OutermostContextBaseType = StateMachine;
  using InnerContextPtrType = StateMachine*;
  using NodeStateBasePtrType = typename detail::StateBase::NodeStateBasePtrType;
  using LeafStatePtrType = typename detail::StateBase::LeafStatePtrType;
  using StateListType = typename detail::StateBase::StateListType;

  void Initiate() {
    Terminate();
    {
      Terminator guard(*this, 0);
      InitialConstruct();
      guard.dismiss();
    }
  }

  void Terminate() {
    Terminator guard(*this, 0);
    TerminateFunction (*this)();
    guard.dismiss();
  }

  bool Terminated() const { return p_outermost_state_ == nullptr; }

  void ProcessEvent(const detail::EventBase& evt) {
    if (SendEvent(evt) == detail::do_defer_event) {
      deferred_event_queue_.emplace_back(evt.shared_from_this());
    }

    ProcessQueuedEvents();
  }

  template <class State>
  void Add(const std::shared_ptr<State>& pState) {
    // The second dummy argument is necessary because the call to the
    // overloaded function add_impl would otherwise be ambiguous.
    NodeStateBasePtrType pNewOutermostUnstableStateCandidate =
        AddImpl(pState, *pState);

    if (is_innermost_common_outer_ ||
        (p_outermost_unstable_state_.get() == pState->State::OuterStatePtr())) {
      is_innermost_common_outer_ = false;
      p_outermost_unstable_state_ = pNewOutermostUnstableStateCandidate;
    }
  }

  void AddInnerState(detail::OrthogonalPositionType position,
                     detail::StateBase* pOutermostState) {
    assert(position == 0);
    position;
    p_outermost_state_ = pOutermostState;
  }

  void RemoveInnerState(detail::OrthogonalPositionType position) {
    assert(position == 0);
    position;
    p_outermost_state_ = nullptr;
  }

  void UnconsumedEvent(const detail::EventBase&) {}

 protected:
  StateMachine()
      : current_states_end_(current_states_.end()),
        p_outermost_state_(nullptr),
        is_innermost_common_outer_(false),
        perform_full_exit_(true),
        p_triggering_event_(nullptr) {}

  // This destructor was only made virtual so that that
  // polymorphic_downcast can be used to cast to MostDerived.
  virtual ~StateMachine() { TerminateImpl(false); }

 private:  // implementation
  void InitialConstruct() {
    InitialState::InitialDeepConstruct(
        *polymorphic_downcast<MostDerived*>(this));
  }

  class TerminateFunction {
   public:
    explicit TerminateFunction(StateMachine& machine) : machine_(machine) {}

    Result operator()() {
      machine_.TerminateImpl(true);
      return detail::do_discard_event;  // there is nothing to be consumed
    }

    TerminateFunction& operator=(const TerminateFunction&) = delete;

   private:
    StateMachine& machine_;
  };
  friend class TerminateFunction;

  class Terminator {
   public:
    Terminator(StateMachine& machine,
               const detail::EventBase* p_new_triggering_event)
        : machine_(machine),
          p_old_triggering_event_(machine_.p_triggering_event_),
          dismissed_(false) {
      machine_.p_triggering_event_ = p_new_triggering_event;
    }

    ~Terminator() {
      if (!dismissed_) {
        machine_.TerminateImpl(false);
      }
      machine_.p_triggering_event_ = p_old_triggering_event_;
    }

    void dismiss() { dismissed_ = true; }

    Terminator& operator=(const Terminator&) = delete;

   private:
    StateMachine& machine_;
    const detail::EventBase* const p_old_triggering_event_;
    bool dismissed_;
  };
  friend class Terminator;

  detail::ReactionResult SendEvent(const detail::EventBase& evt) {
    Terminator guard(*this, &evt);
    assert(p_outermost_unstable_state_ == nullptr);
    const typename detail::RttiPolicy::IdType eventType = evt.dynamic_type();
    detail::ReactionResult reactionResult = detail::do_forward_event;

    for (auto pState = current_states_.begin();
         (reactionResult == detail::do_forward_event) &&
         (pState != current_states_end_);
         ++pState) {
      // CAUTION: The following statement could modify our state list!
      // We must not continue iterating if the event was consumed
      reactionResult = detail::SendFunction(**pState, evt)();
    }

    guard.dismiss();

    if (reactionResult == detail::do_forward_event) {
      polymorphic_downcast<MostDerived*>(this)->UnconsumedEvent(evt);
    }

    return reactionResult;
  }

  void ProcessQueuedEvents() {
    while (!event_queue_.empty()) {
      EventBasePtrType pEvent = event_queue_.front();
      event_queue_.pop_front();

      if (SendEvent(*pEvent) == detail::do_defer_event) {
        deferred_event_queue_.push_back(pEvent);
      }
    }
  }

  void TerminateImpl(bool perform_full_exit) {
    perform_full_exit_ = true;

    if (!Terminated()) {
      TerminateImpl(*p_outermost_state_, perform_full_exit);
    }

    event_queue_.clear();
    deferred_event_queue_.clear();
  }

  void TerminateImpl(detail::StateBase& the_state, bool perform_full_exit) {
    is_innermost_common_outer_ = false;

    // If pOutermostUnstableState_ == 0, we know for sure that
    // currentStates_.size() > 0, otherwise the_state couldn't be alive any
    // more
    if (p_outermost_unstable_state_ != nullptr) {
      the_state.RemoveFromStateList(
          current_states_end_, p_outermost_unstable_state_, perform_full_exit);
    }
    // Optimization: We want to find out whether currentStates_ has size 1
    // and if yes use the optimized implementation below. Since
    // list<>::size() is implemented quite inefficiently in some std libs
    // it is best to just decrement the currentStatesEnd_ here and
    // increment it again, if the test failed.
    else if (current_states_.begin() == --current_states_end_) {
      // The machine is stable and there is exactly one innermost state.
      // The following optimization is only correct for a stable machine
      // without orthogonal regions.
      LeafStatePtrType& pState = *current_states_end_;
      pState->ExitImpl(pState, p_outermost_unstable_state_, perform_full_exit);
    } else {
      assert(current_states_.size() > 1);
      // The machine is stable and there are multiple innermost states
      the_state.RemoveFromStateList(++current_states_end_,
                                    p_outermost_unstable_state_,
                                    perform_full_exit);
    }
  }

  NodeStateBasePtrType AddImpl(const LeafStatePtrType& pState,
                               detail::LeafState&) {
    if (current_states_end_ == current_states_.end()) {
      pState->set_list_position(
          current_states_.insert(current_states_end_, pState));
    } else {
      *current_states_end_ = pState;
      pState->set_list_position(current_states_end_);
      ++current_states_end_;
    }

    return nullptr;
  }

  NodeStateBasePtrType AddImpl(const NodeStateBasePtrType& pState,
                               detail::StateBase&) {
    return pState;
  }

  typedef std::list<EventBasePtrType> EventQueueType;

  EventQueueType event_queue_;
  EventQueueType deferred_event_queue_;
  StateListType current_states_;
  typename StateListType::iterator current_states_end_;
  detail::StateBase* p_outermost_state_;
  bool is_innermost_common_outer_;
  NodeStateBasePtrType p_outermost_unstable_state_;
  bool perform_full_exit_;
  const detail::EventBase* p_triggering_event_;
};

}  // namespace ufsm
