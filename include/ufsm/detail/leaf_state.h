//
// Created by fs on 2/27/21.
//

#pragma once

#include <ufsm/detail/state_base.h>

namespace ufsm {
namespace detail {

class LeafState : public StateBase {
  using BaseType = StateBase;

 protected:
  explicit LeafState(typename RttiPolicy::IdProviderType id_provider)
      : BaseType(id_provider) {}

  ~LeafState() override = default;

 public:
  void set_list_position(
      typename BaseType::StateListType::iterator list_position) {
    list_position_ = list_position;
  }

  using DirectStateBasePtrType = typename BaseType::LeafStatePtrType;

  void RemoveFromStateList(
      typename BaseType::StateListType::iterator& states_end,
      typename BaseType::NodeStateBasePtrType& p_outermost_unstable_state,
      bool perform_full_exit) override {
    --states_end;
    swap(*list_position_, *states_end);
    (*list_position_)->set_list_position(list_position_);
    DirectStateBasePtrType& p_state = *states_end;
    // Because the list owns the leaf_state, this leads to the immediate
    // termination of this state.
    p_state->ExitImpl(p_state, p_outermost_unstable_state, perform_full_exit);
  }

  virtual void ExitImpl(
      DirectStateBasePtrType& p_self,
      typename BaseType::NodeStateBasePtrType& p_outermost_unstable_state,
      bool perform_full_exit) = 0;

 private:
  //////////////////////////////////////////////////////////////////////////_
  typename BaseType::StateListType::iterator list_position_;
};

}  // namespace detail
}  // namespace ufsm
