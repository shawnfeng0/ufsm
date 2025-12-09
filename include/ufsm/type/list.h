#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

namespace ufsm {

// C++17 stdlib compatibility for C++11/14 stdlib
template <typename...>
using void_t = void;

namespace mp {

using std::index_sequence;
using std::make_index_sequence;

struct Na {};

namespace placeholders {
using _ = Na;
}

template <typename... Types>
struct List {};
template <typename HeadType, typename... TailTypes>
struct List<HeadType, TailTypes...> {};

template <typename TypeList>
struct Size;
template <typename... Types>
struct Size<List<Types...>> : std::integral_constant<unsigned, sizeof...(Types)> {};

template <typename TypeList, unsigned int index>
struct At;
template <typename... Types, unsigned int index>
struct At<List<Types...>, index> {
  using type = typename std::tuple_element<index, std::tuple<Types...>>::type;
};

template <typename TypeList>
struct Front;
template <typename Head, typename... Tail>
struct Front<List<Head, Tail...>> {
  using type = Head;
};

template <typename Types, typename T>
struct PushFront;
template <typename... Types, typename T>
struct PushFront<List<Types...>, T> {
  using type = List<T, Types...>;
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

template <bool Condition, typename T1, typename T2>
using If = typename std::conditional<Condition, T1, T2>::type;

template <typename T1>
struct SameAs {
  template <typename T2>
  using apply = std::is_same<T1, T2>;
};

template <typename TypeList, typename Predicate>
struct FindIf;

template <typename Predicate, typename... Types>
struct FindIf<List<Types...>, Predicate> {
  static constexpr int value = []() {
    int idx = 0;
    bool found = ((Predicate::template apply<Types>::value ? true : (idx++, false)) || ...);
    return found ? idx : -1;
  }();
};

template <typename TypeList, typename T>
struct Find : FindIf<TypeList, SameAs<T>> {};

template <typename TypeList, typename T>
struct Contains {
  static constexpr bool value = Find<TypeList, T>::value >= 0;
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

template <typename... Types>
struct Reverse<List<Types...>> {
 private:
  template <std::size_t... I>
  static List<typename std::tuple_element<sizeof...(Types) - 1 - I, std::tuple<Types...>>::type...> helper(
      index_sequence<I...>);

 public:
  using type = decltype(helper(make_index_sequence<sizeof...(Types)>{}));
};

template <typename Types, int begin, int end>
struct Range;

template <typename... Types, int begin, int end>
struct Range<List<Types...>, begin, end> {
 private:
  static constexpr int inner_end = end < 0 ? sizeof...(Types) : end;
  static constexpr std::size_t count = inner_end - begin;

  template <std::size_t... I>
  static List<typename std::tuple_element<I + begin, std::tuple<Types...>>::type...> helper(index_sequence<I...>);

 public:
  using type = decltype(helper(make_index_sequence<count>{}));
};

template <typename List, template <typename> class Op>
struct Transform;

template <typename... Types, template <typename> class Op>
struct Transform<List<Types...>, Op> {
  using type = List<typename Op<Types>::type...>;
};

template <typename TypeList, template <typename> class Predicate>
struct Filter;

template <template <typename> class Predicate, typename... Types>
struct Filter<List<Types...>, Predicate> {
 private:
  template <typename T>
  using MaybeWrap = If<Predicate<T>::value, List<T>, List<>>;

  template <typename... Lists>
  struct Concat {
    using type = List<>;
  };
  template <typename... Ts>
  struct Concat<List<Ts...>> {
    using type = List<Ts...>;
  };
  template <typename... Ts, typename... Us, typename... Rest>
  struct Concat<List<Ts...>, List<Us...>, Rest...> : Concat<List<Ts..., Us...>, Rest...> {};

 public:
  using type = typename Concat<MaybeWrap<Types>...>::type;
};

}  // namespace mp
}  // namespace ufsm
