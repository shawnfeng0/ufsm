#pragma once

#include <ufsm/polymorphic_cast.h>
#include <ufsm/reaction.h>
#include <ufsm/result.h>
#include <ufsm/state_base.h>
#include <ufsm/type_list.h>

#include <iostream>

namespace ufsm {

template <typename MostDerived>
class State : public detail::StateBase {
  struct LocalReactImplNonEmpty {
    template <typename ReactionList, typename State>
    static detail::ReactionResult LocalReactImpl(State& stt,
                                                 const detail::EventBase& evt) {
      detail::ReactionResult reaction_result =
          type::Front<ReactionList>::Type::React(
              *polymorphic_cast<MostDerived*>(&stt), evt);

      if (reaction_result == detail::no_reaction) {
        reaction_result = stt.template LocalReact<
            typename type::PopFront<ReactionList>::Type>(evt);
      }

      return reaction_result;
    }
  };
  friend struct LocalReactImplNonEmpty;

  struct LocalReactImplEmpty {
    template <typename ReactionList, typename State>
    static detail::ReactionResult LocalReactImpl(State&,
                                                 const detail::EventBase&) {
      return detail::do_forward_event;
    }
  };

  template <class ReactionList>
  detail::ReactionResult LocalReact(const detail::EventBase& evt) {
    using impl =
        typename type::If<type::Empty<ReactionList>::value, LocalReactImplEmpty,
                          LocalReactImplNonEmpty>::Type;
    return impl::template LocalReactImpl<ReactionList>(*this, evt);
  }

  detail::ReactionResult ReactImpl(const detail::EventBase& evt) override {
    using reaction_list = typename MostDerived::reactions;
    detail::ReactionResult reaction_result = LocalReact<reaction_list>(evt);

    // At this point we can only safely access pContext_ if the handler did
    // not return do_discard_event!
    if (reaction_result == detail::do_forward_event) {
      // TODO: forward event
      //      reaction_result = pContext_->ReactImpl(evt, eventType);
    }

    return reaction_result;
  }
};

}  // namespace ufsm
