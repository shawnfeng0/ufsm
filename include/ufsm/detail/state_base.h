#pragma once

#include <ufsm/event_base.h>
#include <ufsm/result.h>
#include <ufsm/type/noncopyable.h>
#include <ufsm/type/rtti_policy.h>

#include <memory>
#include <vector>

namespace ufsm {
namespace detail {

class LeafState;
class NodeStateBase;

using OrthogonalPositionType = unsigned char;

class StateBase : public std::enable_shared_from_this<StateBase>, private Noncopyable, RttiPolicy::RttiBaseType {
 public:
  using base_type = RttiPolicy::RttiBaseType;

  virtual const StateBase* OuterStatePtr() const = 0;

 protected:
  explicit StateBase(RttiPolicy::IdType id) : base_type(id), deferred_events_(false) {}
  ~StateBase() = default;

 protected:
  void defer_event() noexcept { deferred_events_ = true; }
  bool deferred_events() const noexcept { return deferred_events_; }

  template <class ContextPtr>
  void SetContext(OrthogonalPositionType position, const ContextPtr& p_context) {
    p_context->AddInnerState(position, this);
  }

 public:
  virtual ReactionResult ReactImpl(const EventBase& evt) = 0;

  using NodeStateBasePtrType = std::shared_ptr<NodeStateBase>;
  using LeafStatePtrType = std::shared_ptr<LeafState>;
  using StateListType = std::vector<LeafStatePtrType>;

  std::shared_ptr<StateBase> get_shared_ptr() { return shared_from_this(); }
  std::weak_ptr<StateBase> get_weak_ptr() { return shared_from_this(); }

  virtual void RemoveFromStateList(StateListType& states, std::size_t& states_end_index,
                                   NodeStateBasePtrType& pOutermostUnstableState, bool perform_full_exit) = 0;

 private:
  bool deferred_events_;
};

}  // namespace detail
}  // namespace ufsm
