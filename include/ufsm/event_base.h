//
// Created by fs on 2021-02-22.
//

#pragma once

#include <ufsm/type/rtti_policy.h>

#include <memory>

namespace ufsm {
namespace detail {

class EventBase : public RttiPolicy::RttiBaseType {
 protected:
  explicit EventBase(RttiPolicy::IdProviderType id_provider)
      : RttiPolicy::RttiBaseType(id_provider){};
  virtual ~EventBase() = default;

 public:
  std::shared_ptr<const detail::EventBase> shared_from_this() const {
    return clone();
  }

 private:
  virtual std::shared_ptr<const EventBase> clone() const = 0;
};

}  // namespace detail
}  // namespace ufsm
