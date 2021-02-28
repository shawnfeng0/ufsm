//
// Created by fs on 2021-02-23.
//

#pragma once

namespace ufsm {
namespace detail {

template <typename T1, typename T2>
struct IsSameType {
  constexpr explicit operator bool() { return false; }
};

template <typename T>
struct IsSameType<T, T> {
  constexpr explicit operator bool() { return true; }
};

template <typename T>
struct ContextContainer {
  T *ctx_ptr_{};
};

template <typename T>
static void *Constructor() {
  return new T;
}

class EmptyContext {};

}  // namespace detail
}  // namespace ufsm
