#pragma once

#include <ufsm/detail/state_base.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>

namespace ufsm {
namespace detail {

class NodeStateBase : public StateBase {
  using BaseType = StateBase;

 protected:
  explicit NodeStateBase(RttiPolicy::IdType id) : BaseType(id) {}

  ~NodeStateBase() = default;

 public:
  using StateBaseType = BaseType;

  using DirectStateBasePtrType = std::shared_ptr<NodeStateBase>;
  virtual void ExitImpl(DirectStateBasePtrType& pSelf, typename BaseType::NodeStateBasePtrType& pOutermostUnstableState,
                        bool performFullExit) = 0;
};

template <OrthogonalPositionType OrthogonalRegionCount = 1>
class NodeState : public NodeStateBase {
  using BaseType = NodeStateBase;

 protected:
  explicit NodeState(RttiPolicy::IdType id) : BaseType(id) { p_inner_states_.fill(nullptr); }

  ~NodeState() = default;

 public:
  using StateBaseType = typename BaseType::StateBaseType;

  void AddInnerState(OrthogonalPositionType position, StateBaseType* pInnerState) noexcept {
    assert((position < OrthogonalRegionCount) && (p_inner_states_[position] == nullptr));
    p_inner_states_[position] = pInnerState;
  }

  void RemoveInnerState(OrthogonalPositionType position) noexcept {
    assert(position < OrthogonalRegionCount);
    p_inner_states_[position] = nullptr;
  }

  void RemoveFromStateList(typename StateBaseType::StateListType& states, std::size_t& states_end_index,
                           typename StateBaseType::NodeStateBasePtrType& p_outermost_unstable_state,
                           bool perform_full_exit) override {
    auto p_past_end = p_inner_states_.end();
    auto p_first_non_null =
        std::find_if(p_inner_states_.begin(), p_inner_states_.end(), [](auto* p) { return p != nullptr; });

    if (p_first_non_null == p_past_end) {
      // The state does not have inner states but is still alive, this must
      // be the outermost unstable state then.
      assert(p_outermost_unstable_state.get() == this);
      typename StateBaseType::NodeStateBasePtrType p_self = p_outermost_unstable_state;
      p_self->ExitImpl(p_self, p_outermost_unstable_state, perform_full_exit);
    } else {
      // Destroy inner states in the reverse order of construction
      for (auto pState = p_past_end; pState != p_first_non_null;) {
        --pState;

        // An inner orthogonal state might have been terminated long before,
        // that's why we have to check for 0 pointers
        if (*pState != nullptr) {
          (*pState)->RemoveFromStateList(states, states_end_index, p_outermost_unstable_state, perform_full_exit);
        }
      }
    }
  }

  using DirectStateBasePtrType = typename BaseType::DirectStateBasePtrType;

 private:
  std::array<StateBaseType*, OrthogonalRegionCount> p_inner_states_;
};

}  // namespace detail
}  // namespace ufsm
