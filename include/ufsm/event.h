//
// Created by fs on 2021-02-22.
//

#pragma once

#include <ufsm/detail/polymorphic_cast.h>
#include <ufsm/event_base.h>
#include <ufsm/type/rtti_policy.h>

#include <memory>

namespace ufsm {

template <typename MostDerived>
class Event : public detail::RttiPolicy::RttiDerivedType<MostDerived,
                                                         detail::EventBase> {
 private:
  virtual std::shared_ptr<const detail::EventBase> clone() const {
    return std::shared_ptr<const detail::EventBase>(
        new MostDerived(*polymorphic_downcast<const MostDerived*>(this)));
  }
};

}  // namespace ufsm
