#pragma once
#include <ufsm/result.h>

#include <type_traits>

namespace ufsm {

template <class Event>
class Reaction {
 public:
  template <typename State, typename EventBase>
  static detail::ReactionResult React(State& stt, const EventBase& evt) {
    if (evt.dynamic_type() == Event::static_type()) {
      const auto& casted_evt = *static_cast<const Event*>(&evt);
      if constexpr (std::is_void<decltype(stt.React(casted_evt))>::value) {
        stt.React(casted_evt);
        return detail::do_discard_event;
      } else {
        return stt.React(casted_evt);
      }
    }
    return detail::no_reaction;
  }
};

}  // namespace ufsm
