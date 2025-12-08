#pragma once

#include <ufsm/event_base.h>

#include <memory>

namespace ufsm {

template <typename MostDerived>
class Event : public detail::RttiPolicy::RttiDerivedType<MostDerived, detail::EventBase> {
 private:
  std::shared_ptr<const detail::EventBase> clone() const override {
    return std::make_shared<MostDerived>(*static_cast<const MostDerived*>(this));
  }
};

}  // namespace ufsm
