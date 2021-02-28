#pragma once

#include <ufsm/event.h>
#include <ufsm/event_base.h>
#include <ufsm/result.h>
#include <ufsm/rtti_policy.h>

namespace ufsm {
namespace detail {

class StateBase {
 public:
  using reactions = type::List<>;
  virtual detail::ReactionResult ReactImpl(const EventBase& evt) = 0;
};

}  // namespace detail
}  // namespace ufsm
