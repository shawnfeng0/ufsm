#pragma once

#include <ufsm/detail/leaf_state.h>
#include <ufsm/detail/state_base.h>
#include <ufsm/event_base.h>
#include <ufsm/type/list.h>

#include <cassert>
#include <deque>

namespace ufsm {

template <typename MostDerived, typename InitialState>
class StateMachine {
 public:
  using EventBasePtrType = std::shared_ptr<const detail::EventBase>;

  using InnerContextType = MostDerived;

  using InnerOrthogonalPosition = std::integral_constant<detail::OrthogonalPositionType, 0>;
  using NoOfOrthogonalRegions = std::integral_constant<detail::OrthogonalPositionType, 1>;

  using OutermostContextType = MostDerived;
  using OutermostContextBaseType = StateMachine;
  using InnerContextPtrType = StateMachine*;
  using NodeStateBasePtrType = typename detail::StateBase::NodeStateBasePtrType;
  using LeafStatePtrType = typename detail::StateBase::LeafStatePtrType;
  using StateListType = typename detail::StateBase::StateListType;
  using ContextTypeList = mp::List<>;

  void Initiate() {
    Terminate();
    {
      Terminator guard(*this, 0);
      InitialConstruct();
      guard.dismiss();
    }
  }

  void Terminate() {
    Terminator guard(*this, nullptr);
    TerminateImpl(true);
    guard.dismiss();
  }

  bool Terminated() const noexcept { return p_outermost_state_ == nullptr; }

  void ProcessEvent(const detail::EventBase& evt) {
    if (SendEvent(evt) == detail::do_defer_event) {
      deferred_event_queue_.emplace_back(evt.shared_from_this());
    }

    ProcessQueuedEvents();
  }

  void ProcessEvent(const EventBasePtrType& p_evt) {
    if (SendEvent(*p_evt) == detail::do_defer_event) {
      deferred_event_queue_.push_back(p_evt);
    }

    ProcessQueuedEvents();
  }

  void ExitImpl(InnerContextPtrType&, typename detail::StateBase::NodeStateBasePtrType&, bool) {}

  void SetOutermostUnstableState(typename detail::StateBase::NodeStateBasePtrType& p_outermost_unstable_state) {
    p_outermost_unstable_state = nullptr;
  }

  // Returns a reference to the context identified by the template
  // parameter. This can either be _this_ object or one of its direct or
  // indirect contexts.
  template <class ContextType>
  ContextType& Context() {
    return *static_cast<MostDerived*>(this);
  }

  template <class ContextType>
  const ContextType& Context() const {
    return *static_cast<const MostDerived*>(this);
  }

  OutermostContextType& OutermostContext() { return *static_cast<MostDerived*>(this); }

  const OutermostContextType& OutermostContext() const { return *static_cast<const MostDerived*>(this); }

  OutermostContextBaseType& OutermostContextBase() noexcept { return *this; }

  const OutermostContextBaseType& OutermostContextBase() const noexcept { return *this; }

  void TerminateAsReaction(detail::StateBase& theState) {
    TerminateImpl(theState, perform_full_exit_);
    p_outermost_unstable_state_ = nullptr;
  }

  void TerminateAsPartOfTransit(detail::StateBase& the_state) {
    TerminateImpl(the_state, perform_full_exit_);
    is_innermost_common_outer_ = true;
  }

  void TerminateAsPartOfTransit(StateMachine&) {
    TerminateImpl(*p_outermost_state_, perform_full_exit_);
    is_innermost_common_outer_ = true;
  }

  detail::ReactionResult ReactImpl(const detail::EventBase&) { return detail::do_forward_event; }

  template <class State>
  void Add(const std::shared_ptr<State>& p_state) {
    NodeStateBasePtrType pNewOutermostUnstableStateCandidate = AddImpl(p_state, *p_state);

    if (is_innermost_common_outer_ || (p_outermost_unstable_state_.get() == p_state->State::OuterStatePtr())) {
      is_innermost_common_outer_ = false;
      p_outermost_unstable_state_ = pNewOutermostUnstableStateCandidate;
    }
  }

  void AddInnerState(detail::OrthogonalPositionType position, detail::StateBase* p_outermost_state) {
    assert(position == 0);
    (void)position;
    p_outermost_state_ = p_outermost_state;
  }

  void RemoveInnerState(detail::OrthogonalPositionType position) {
    assert(position == 0);
    (void)position;
    p_outermost_state_ = nullptr;
  }

  void UnconsumedEvent(const detail::EventBase&) {}

 protected:
  StateMachine()
      : current_states_active_count_(0),
        p_outermost_state_(nullptr),
        is_innermost_common_outer_(false),
        perform_full_exit_(true),
        p_triggering_event_(nullptr) {}

  virtual ~StateMachine() { TerminateImpl(false); }

 private:  // implementation
  void InitialConstruct() { InitialState::InitialDeepConstruct(*static_cast<MostDerived*>(this)); }

  class Terminator {
   public:
    Terminator(StateMachine& machine, const detail::EventBase* p_new_triggering_event)
        : machine_(machine), p_old_triggering_event_(machine_.p_triggering_event_), dismissed_(false) {
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
    detail::ReactionResult reaction_result = detail::do_forward_event;

    for (std::size_t i = 0; (reaction_result == detail::do_forward_event) && (i < current_states_active_count_); ++i) {
      // CAUTION: The following statement could modify our state list!
      // We must not continue iterating if the event was consumed
      reaction_result = current_states_[i]->ReactImpl(evt);
    }

    guard.dismiss();

    if (reaction_result == detail::do_forward_event) {
      static_cast<MostDerived*>(this)->UnconsumedEvent(evt);
    }

    return reaction_result;
  }

  void ProcessQueuedEvents() {
    while (!event_queue_.empty()) {
      EventBasePtrType p_event = event_queue_.front();
      event_queue_.pop_front();

      if (SendEvent(*p_event) == detail::do_defer_event) {
        deferred_event_queue_.push_back(p_event);
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
      the_state.RemoveFromStateList(current_states_, current_states_active_count_, p_outermost_unstable_state_,
                                    perform_full_exit);
    }
    // Optimization: We want to find out whether currentStates_ has size 1
    // and if yes use the optimized implementation below.
    else if (current_states_active_count_ == 1) {
      // The machine is stable and there is exactly one innermost state.
      // The following optimization is only correct for a stable machine
      // without orthogonal regions.
      current_states_active_count_ = 0;
      LeafStatePtrType& p_state = current_states_[0];
      p_state->ExitImpl(p_state, p_outermost_unstable_state_, perform_full_exit);
    } else {
      assert(current_states_active_count_ > 1);
      // The machine is stable and there are multiple innermost states
      the_state.RemoveFromStateList(current_states_, current_states_active_count_, p_outermost_unstable_state_,
                                    perform_full_exit);
    }
  }

  NodeStateBasePtrType AddImpl(const LeafStatePtrType& p_state, detail::LeafState&) {
    if (current_states_active_count_ == current_states_.size()) {
      p_state->set_list_index(current_states_.size());
      current_states_.push_back(p_state);
    } else {
      current_states_[current_states_active_count_] = p_state;
      p_state->set_list_index(current_states_active_count_);
    }
    ++current_states_active_count_;
    return nullptr;
  }

  NodeStateBasePtrType AddImpl(const NodeStateBasePtrType& p_state, detail::StateBase&) { return p_state; }

  using EventQueueType = std::deque<EventBasePtrType>;

  EventQueueType event_queue_;
  EventQueueType deferred_event_queue_;
  StateListType current_states_;
  std::size_t current_states_active_count_;
  detail::StateBase* p_outermost_state_;
  bool is_innermost_common_outer_;
  NodeStateBasePtrType p_outermost_unstable_state_;
  bool perform_full_exit_;
  const detail::EventBase* p_triggering_event_;
};

}  // namespace ufsm
