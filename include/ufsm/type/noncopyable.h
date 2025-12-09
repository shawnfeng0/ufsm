#pragma once

namespace ufsm {
namespace detail {

class Noncopyable {
 public:
  Noncopyable(const Noncopyable&) = delete;
  Noncopyable& operator=(const Noncopyable&) = delete;

 protected:
  Noncopyable() noexcept = default;
  ~Noncopyable() noexcept = default;
};

}  // namespace detail
}  // namespace ufsm
