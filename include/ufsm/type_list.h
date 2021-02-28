//
// Created by fs on 2021-02-23.
//

#pragma once

// Reference: https://www.cnblogs.com/apocelipes/p/11289840.html

namespace ufsm {
namespace type {

template <typename... Types>
struct List;
template <>
struct List<> {};
template <typename HeadType, typename... TailTypes>
struct List<HeadType, TailTypes...> {
  using head_type = HeadType;
  using tail_types = List<TailTypes...>;
};

template <typename... Types>
struct ListLength {
  static constexpr unsigned value = sizeof...(Types);
};
template <typename... Types>
struct ListLength<List<Types...>> {
  static constexpr unsigned value = sizeof...(Types);
};

template <typename TypeList, unsigned int index>
struct At;
template <typename HeadType, typename... Types>
struct At<List<HeadType, Types...>, 0> {
  using Type = HeadType;
};
template <typename HeadType, typename... Types, unsigned int index>
struct At<List<HeadType, Types...>, index> {
  static_assert(index < sizeof...(Types) + 1, "i out of range");
  using Type = typename At<List<Types...>, index - 1>::Type;
};

template <typename TypeList>
struct Front;
template <typename... Types>
struct Front<List<Types...>> {
  using Type = typename At<List<Types...>, 0>::Type;
};

template <typename Types, typename T>
struct Append;
template <typename... Types, typename T>
struct Append<List<Types...>, T> {
  using Type = List<T, Types...>;
};

template <typename Types>
struct PopFront;
template <typename... Types, typename HeadType>
struct PopFront<List<HeadType, Types...>> {
  using Type = List<Types...>;
};
template <>
struct PopFront<List<>> {
  using Type = List<>;
};

template <typename Types>
struct Empty;
template <>
struct Empty<List<>> {
  constexpr explicit operator bool() const { return true; }
  static constexpr bool value = true;
};
template <typename... Types>
struct Empty<List<Types...>> {
  constexpr explicit operator bool() const { return false; }
  static constexpr bool value = false;
};

template <bool Condition, typename T1, typename T2>
struct If;
template <typename T1, typename T2>
struct If<true, T1, T2> {
  using Type = T1;
};
template <typename T1, typename T2>
struct If<false, T1, T2> {
  using Type = T2;
};

}  // namespace type
}  // namespace ufsm
