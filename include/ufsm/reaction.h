#pragma once
#include <ufsm/polymorphic_cast.h>
#include <ufsm/result.h>

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
      return stt.React(*polymorphic_cast<const Event*>(&evt));
    } else {
      return detail::no_reaction;
    }
  }
};

}  // namespace ufsm
