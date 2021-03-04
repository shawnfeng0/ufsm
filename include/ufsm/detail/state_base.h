#pragma once

#include <ufsm/event.h>
#include <ufsm/event_base.h>
#include <ufsm/result.h>
#include <ufsm/type/noncopyable.h>
#include <ufsm/type/rtti_policy.h>

#include <list>
#include <memory>

namespace ufsm {
namespace detail {

class LeafState;
class NodeStateBase;

using OrthogonalPositionType = unsigned char;

class StateBase : public std::enable_shared_from_this<StateBase>,
                  private detail::Noncopyable,
                  RttiPolicy::RttiBaseType {
 public:
  using base_type = RttiPolicy::RttiBaseType;

  void Exit() {}  // TODO: unused
  virtual const StateBase* OuterStatePtr() const = 0;

 protected:
  explicit StateBase(typename RttiPolicy::IdProviderType id_provider)
      : base_type(id_provider), deferred_events_(false) {}
  ~StateBase() = default;

 protected:
  void defer_event() { deferred_events_ = true; }
  bool deferred_events() const { return deferred_events_; }

  template <class ContextPtr>
  void SetContext(OrthogonalPositionType position, ContextPtr p_context) {
    p_context->AddInnerState(position, this);
  }

 public:
  virtual ReactionResult ReactImpl(const EventBase& evt) = 0;

  using NodeStateBasePtrType = std::shared_ptr<NodeStateBase>;
  using LeafStatePtrType = std::shared_ptr<LeafState>;
  using StateListType = std::list<LeafStatePtrType>;

  std::shared_ptr<StateBase> get_shared_ptr() { return shared_from_this(); }
  std::weak_ptr<StateBase> get_weak_ptr() { return weak_from_this(); }

  virtual void RemoveFromStateList(
      typename StateListType::iterator& statesEnd,
      NodeStateBasePtrType& pOutermostUnstableState,
      bool perform_full_exit) = 0;

 private:
  bool deferred_events_;
};

}  // namespace detail
}  // namespace ufsm
