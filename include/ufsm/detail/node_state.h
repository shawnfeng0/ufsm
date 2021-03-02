//
// Created by fs on 2/27/21.
//

#include <ufsm/detail/state_base.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>

#pragma once

namespace ufsm {
namespace detail {

class NodeStateBase : public StateBase {
  using BaseType = StateBase;

 protected:
  explicit NodeStateBase(typename RttiPolicy::IdProviderType id_provider)
      : BaseType(id_provider) {}

  ~NodeStateBase() override = default;

 public:
  using StateBaseType = BaseType;

  typedef std::shared_ptr<NodeStateBase> DirectStateBasePtrType;
  virtual void ExitImpl(
      DirectStateBasePtrType& pSelf,
      typename BaseType::NodeStateBasePtrType& pOutermostUnstableState,
      bool performFullExit) = 0;
};

template <OrthogonalPositionType OrthogonalRegionCount = 1>
class NodeState : public NodeStateBase {
  using BaseType = NodeStateBase;

 protected:
  //////////////////////////////////////////////////////////////////////////
  explicit NodeState(typename RttiPolicy::IdProviderType idProvider)
      : BaseType(idProvider) {
    for (OrthogonalPositionType pos = 0; pos < OrthogonalRegionCount; ++pos) {
      p_inner_states_[pos] = 0;
    }
  }

  ~NodeState() override = default;

 public:
  using StateBaseType = typename BaseType::StateBaseType;

  void AddInnerState(OrthogonalPositionType position,
                     StateBaseType* pInnerState) {
    assert((position < OrthogonalRegionCount) &&
           (p_inner_states_[position] == 0));
    p_inner_states_[position] = pInnerState;
  }

  void RemoveInnerState(OrthogonalPositionType position) {
    assert(position < OrthogonalRegionCount);
    p_inner_states_[position] = 0;
  }

  void RemoveFromStateList(
      typename StateBaseType::StateListType::iterator& states_end,
      typename StateBaseType::NodeStateBasePtrType& p_outermost_unstable_state,
      bool perform_full_exit) override {
    auto p_past_end = p_inner_states_.end();
    auto p_first_non_null =
        std::find_if(p_inner_states_.begin(), p_inner_states_.end(),
                     &NodeState::is_not_null);

    if (p_first_non_null == p_past_end) {
      // The state does not have inner states but is still alive, this must
      // be the outermost unstable state then.
      assert(p_outermost_unstable_state.get() == this);
      typename StateBaseType::NodeStateBasePtrType p_self =
          p_outermost_unstable_state;
      p_self->ExitImpl(p_self, p_outermost_unstable_state, perform_full_exit);

    } else {
      // Destroy inner states in the reverse order of construction
      for (auto pState = p_past_end; pState != p_first_non_null;) {
        --pState;

        // An inner orthogonal state might have been terminated long before,
        // that's why we have to check for 0 pointers
        if (*pState != nullptr) {
          (*pState)->RemoveFromStateList(states_end, p_outermost_unstable_state,
                                         perform_full_exit);
        }
      }
    }
  }

  using DirectStateBasePtrType = typename BaseType::DirectStateBasePtrType;

 private:
  static bool is_not_null(const StateBaseType* pInner) {
    return pInner != nullptr;
  }
  std::array<StateBaseType*, OrthogonalRegionCount> p_inner_states_{};
};

}  // namespace detail
}  // namespace ufsm
