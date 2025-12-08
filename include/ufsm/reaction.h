#pragma once
#include <ufsm/detail/polymorphic_cast.h>
#include <ufsm/result.h>
#include <type_traits>

namespace ufsm {

template <class Event>
class Reaction {
 public:
  //////////////////////////////////////////////////////////////////////////
  // The following declarations should be private.
  // They are only public because many compilers lack template friends.
  //////////////////////////////////////////////////////////////////////////
  template <typename State, typename EventBase>
  static detail::ReactionResult React(State& stt, const EventBase& evt) {
    if (evt.dynamic_type() == Event::static_type()) {
      if constexpr (std::is_void_v<decltype(stt.React(*polymorphic_cast<const Event*>(&evt)))>) {
        stt.React(*polymorphic_cast<const Event*>(&evt));
        return detail::do_discard_event;
      } else {
        return stt.React(*polymorphic_cast<const Event*>(&evt));
      }
    } else {
      return detail::no_reaction;
    }
  }
};

}  // namespace ufsm
