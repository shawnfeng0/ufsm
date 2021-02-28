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

class StateBase : detail::Noncopyable, RttiPolicy::RttiBaseType {
 public:
  using base_type = RttiPolicy::RttiBaseType;

  void Exit() {}  // TODO: unused
  virtual const StateBase* OuterStatePtr() const = 0;

 protected:
  explicit StateBase(typename RttiPolicy::IdProviderType id_provider)
      : base_type(id_provider), deferred_events_(false) {}
  virtual ~StateBase() = default;

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

  virtual void RemoveFromStateList(
      typename StateListType::iterator& statesEnd,
      NodeStateBasePtrType& pOutermostUnstableState,
      bool perform_full_exit) = 0;

 private:
  bool deferred_events_;
};

}  // namespace detail
}  // namespace ufsm
