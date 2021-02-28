//
// Created by fs on 2021-02-26.
//

#pragma once

namespace ufsm {
namespace detail {

class Noncopyable {
 public:
  Noncopyable(const Noncopyable&) = delete;
  Noncopyable& operator=(const Noncopyable&) = delete;

 protected:
  Noncopyable() = default;
  ~Noncopyable() = default;
};

}  // namespace detail
}  // namespace ufsm
