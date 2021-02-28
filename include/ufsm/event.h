//
// Created by fs on 2021-02-22.
//

#pragma once

#include <ufsm/event_base.h>
#include <ufsm/rtti_policy.h>

namespace ufsm {

template <typename MostDerived>
class Event : public detail::RttiPolicy::RttiDerivedType<MostDerived,
                                                         detail::EventBase> {};

}  // namespace ufsm
