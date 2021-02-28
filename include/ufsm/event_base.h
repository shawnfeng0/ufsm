//
// Created by fs on 2021-02-22.
//

#pragma once

#include <ufsm/type/rtti_policy.h>

namespace ufsm {
namespace detail {

class EventBase : public RttiPolicy::RttiBaseType {
 protected:
  explicit EventBase(RttiPolicy::IdProviderType id_provider)
      : RttiPolicy::RttiBaseType(id_provider){};
  virtual ~EventBase() = default;
};

}  // namespace detail
}  // namespace ufsm
