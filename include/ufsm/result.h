#pragma once

namespace ufsm {
namespace detail {

enum ReactionResult { no_reaction, do_forward_event, do_discard_event, do_defer_event, consumed };

}

using Result = detail::ReactionResult;

}  // namespace ufsm
