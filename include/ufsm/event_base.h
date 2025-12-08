#pragma once

#include <ufsm/type/rtti_policy.h>

#include <memory>

namespace ufsm {
namespace detail {

class EventBase : public RttiPolicy::RttiBaseType {
 protected:
  explicit EventBase(RttiPolicy::IdType id) : RttiPolicy::RttiBaseType(id) {};
  virtual ~EventBase() = default;

 public:
  std::shared_ptr<const detail::EventBase> shared_from_this() const { return clone(); }

 private:
  virtual std::shared_ptr<const EventBase> clone() const = 0;
};

}  // namespace detail
}  // namespace ufsm
