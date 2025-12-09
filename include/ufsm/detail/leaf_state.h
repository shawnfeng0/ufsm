#pragma once

#include <ufsm/detail/state_base.h>

#include <utility>

namespace ufsm {
namespace detail {

class LeafState : public StateBase {
  using BaseType = StateBase;

 protected:
  explicit LeafState(RttiPolicy::IdType id) : BaseType(id) {}

  ~LeafState() = default;

 public:
  void set_list_index(std::size_t list_index) noexcept { list_index_ = list_index; }

  using DirectStateBasePtrType = typename BaseType::LeafStatePtrType;

  void RemoveFromStateList(typename BaseType::StateListType& states, std::size_t& states_end_index,
                           typename BaseType::NodeStateBasePtrType& p_outermost_unstable_state,
                           bool perform_full_exit) override {
    --states_end_index;
    using std::swap;
    swap(states[list_index_], states[states_end_index]);
    states[list_index_]->set_list_index(list_index_);
    DirectStateBasePtrType& p_state = states[states_end_index];
    // Because the list owns the leaf_state, this leads to the immediate
    // termination of this state.
    p_state->ExitImpl(p_state, p_outermost_unstable_state, perform_full_exit);
  }

  virtual void ExitImpl(DirectStateBasePtrType& p_self,
                        typename BaseType::NodeStateBasePtrType& p_outermost_unstable_state,
                        bool perform_full_exit) = 0;

 private:
  std::size_t list_index_;
};

}  // namespace detail
}  // namespace ufsm
