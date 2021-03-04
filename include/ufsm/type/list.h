//
// Created by fs on 2021-02-23.
//

#pragma once
#include <type_traits>

// Reference: https://www.cnblogs.com/apocelipes/p/11289840.html

namespace ufsm {
namespace mp {

struct Na {
  using type = Na;
};

namespace placeholders {
using _ = Na;
}

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
struct IsList {
  static constexpr bool value = false;
};

template <typename... Types>
struct IsList<List<Types...>> {
  static constexpr bool value = true;
};

template <typename... Types>
struct Size {
  static constexpr unsigned value = sizeof...(Types);
};
template <typename... Types>
struct Size<List<Types...>> {
  static constexpr unsigned value = sizeof...(Types);
};

template <typename TypeList, unsigned int index>
struct At;
template <typename HeadType, typename... Types>
struct At<List<HeadType, Types...>, 0> {
  using type = HeadType;
};
template <typename HeadType, typename... Types, unsigned int index>
struct At<List<HeadType, Types...>, index> {
  static_assert(index < sizeof...(Types) + 1, "i out of range");
  using type = typename At<List<Types...>, index - 1>::type;
};

template <typename TypeList>
struct Front;
template <typename... Types>
struct Front<List<Types...>> {
  using type = typename At<List<Types...>, 0>::type;
};

template <typename Types, typename T>
struct PushFront;
template <typename... Types, typename T>
struct PushFront<List<Types...>, T> {
  using type = List<T, Types...>;
};

template <typename Types, typename T>
struct PushBack;
template <typename... Types, typename T>
struct PushBack<List<Types...>, T> {
  using type = List<Types..., T>;
};

template <typename Types>
struct PopFront;
template <typename... Types, typename HeadType>
struct PopFront<List<HeadType, Types...>> {
  using type = List<Types...>;
};
template <>
struct PopFront<List<>> {
  using type = List<>;
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
  using type = T1;
};
template <typename T1, typename T2>
struct If<false, T1, T2> {
  using type = T2;
};

template <typename T1>
struct SameAs {
  template <typename T2>
  struct apply : std::is_same<T1, T2> {};
};

template <typename TypeList, typename Predicate>
struct FindIf;
template <typename Predicate>
struct FindIf<List<>, Predicate> {
  static constexpr int value = -1;
};
template <typename... Types, typename Predicate>
struct FindIf<List<Types...>, Predicate> {
 private:
  static constexpr int increase(int raw) { return raw >= 0 ? raw + 1 : raw; }

 public:
  static constexpr int value =
      If<Predicate::template apply<typename Front<List<Types...>>::type>::value,
         std::integral_constant<int, 0>,
         std::integral_constant<
             int, increase(FindIf<typename PopFront<List<Types...>>::type,
                                  Predicate>::value)>>::type::value;
};

template <typename TypeList, typename T>
struct Find : FindIf<TypeList, SameAs<T>> {};

template <typename TypeList, typename T>
struct Contains {
 private:
  static constexpr bool is_non_negative(int n) { return n >= 0; }

 public:
  static constexpr bool value = is_non_negative(Find<TypeList, T>::value);
};
template <typename TypeList>
struct Contains<TypeList, placeholders::_> {  // For function
  template <typename T>
  struct apply {
    static constexpr bool value = Contains<TypeList, T>::value;
  };
};

template <typename T>
struct Reverse;
template <>
struct Reverse<List<>> {
  using type = List<>;
};
template <typename T, typename... Args>
struct Reverse<List<T, Args...>> {
  using type =
      typename PushBack<typename Reverse<List<Args...>>::type, T>::type;
};

template <typename Types, typename ResultTypes, int begin, int end, int cur>
struct RangeImpl;
template <typename... Types, typename ResultTypes, int begin, int end>
struct RangeImpl<List<Types...>, ResultTypes, begin, end, end> {
  using type = ResultTypes;
};
template <typename... Types, typename ResultTypes, int begin, int end, int cur>
struct RangeImpl<List<Types...>, ResultTypes, begin, end, cur> {
  static_assert(begin >= 0 && begin < end, "");

 private:
  using InnerList = List<Types...>;
  static constexpr bool index_valid() { return cur >= begin && cur < end; }

 public:
  using type =
      typename If<index_valid(),
                  typename RangeImpl<
                      InnerList,
                      typename PushBack<
                          ResultTypes, typename At<InnerList, cur>::type>::type,
                      begin, end, cur + 1>::type,
                  typename RangeImpl<InnerList, ResultTypes, begin, end,
                                     cur + 1>::type>::type;
};

template <typename Types, int begin, int end>
struct Range;
template <typename... Types, int begin, int end>
struct Range<List<Types...>, begin, end> {
 private:
  static constexpr int inner_end = end < 0 ? sizeof...(Types) : end;

 public:
  using type =
      typename RangeImpl<List<Types...>, List<>, begin, inner_end, 0>::type;
};

}  // namespace mp
}  // namespace ufsm
