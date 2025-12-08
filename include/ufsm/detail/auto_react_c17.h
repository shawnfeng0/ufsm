#pragma once

#include <ufsm/type/list.h>
#include <ufsm/reaction.h>
#include <type_traits>
#include <utility>

namespace ufsm {
namespace detail {

// --- Meta-programming helpers missing in list.h ---

// Filter
template <typename List, template <typename> class Predicate>
struct Filter;

template <template <typename> class Predicate>
struct Filter<mp::List<>, Predicate> {
  using type = mp::List<>;
};

template <typename Head, typename... Tail, template <typename> class Predicate>
struct Filter<mp::List<Head, Tail...>, Predicate> {
  using type = typename mp::If<
      Predicate<Head>::value,
      typename mp::PushFront<typename Filter<mp::List<Tail...>, Predicate>::type, Head>::type,
      typename Filter<mp::List<Tail...>, Predicate>::type
  >::type;
};

// Transform
template <typename List, template <typename> class Op>
struct Transform;

template <template <typename> class Op>
struct Transform<mp::List<>, Op> {
    using type = mp::List<>;
};

template <typename Head, typename... Tail, template <typename> class Op>
struct Transform<mp::List<Head, Tail...>, Op> {
    using type = typename mp::PushFront<typename Transform<mp::List<Tail...>, Op>::type, typename Op<Head>::type>::type;
};

// --- Detection of React method ---

template <typename T, typename E, typename = void>
struct HasReactMethod : std::false_type {};

// Detects T::React(const E&)
template <typename T, typename E>
struct HasReactMethod<T, E, std::void_t<decltype(std::declval<T>().React(std::declval<const E&>()))>> : std::true_type {};

// --- Auto Generation of Reactions ---

template <typename E>
struct MakeReaction {
    using type = Reaction<E>;
};

template <typename State, typename EventList>
struct AutoReactions {
    template <typename E>
    using Predicate = HasReactMethod<State, E>;

    using FilteredEvents = typename Filter<EventList, Predicate>::type;
    using type = typename Transform<FilteredEvents, MakeReaction>::type;
};

struct AutoReactionsTag {};

// --- EventList Trait for breaking circular dependencies ---
} // namespace detail

template <typename T>
struct EventListTrait {
    using type = void;
};

namespace detail {

// --- Helper to get reactions or fallback ---


template <typename T, typename = void>
struct HasReactionsTypedef : std::false_type {};

template <typename T>
struct HasReactionsTypedef<T, std::void_t<typename T::reactions>> : std::true_type {};

template <typename T, typename = void>
struct HasEventListTypedef : std::false_type {};

template <typename T>
struct HasEventListTypedef<T, std::void_t<typename T::EventList>> : std::true_type {};

// Helper to find EventList in Context (recursively?)
// For now, let's look in MostDerived::EventList, then Context::EventList.

template <typename State, typename Context, typename = void>
struct GetEventList {
    using type = mp::List<>; // Default empty
};

// Priority 1: EventListTrait<Context>
template <typename State, typename Context>
struct GetEventList<State, Context, std::enable_if_t<!std::is_same<typename ufsm::EventListTrait<Context>::type, void>::value>> {
    using type = typename ufsm::EventListTrait<Context>::type;
};

// Priority 2: State::EventList
template <typename State, typename Context>
struct GetEventList<State, Context, std::enable_if_t<std::is_same<typename ufsm::EventListTrait<Context>::type, void>::value && HasEventListTypedef<State>::value>> {
    using type = typename State::EventList;
};

// Priority 3: Context::EventList
template <typename State, typename Context>
struct GetEventList<State, Context, std::enable_if_t<std::is_same<typename ufsm::EventListTrait<Context>::type, void>::value && !HasEventListTypedef<State>::value && HasEventListTypedef<Context>::value>> {
    using type = typename Context::EventList;
};

template <typename State, typename Context>
struct GetReactions {
    // If State::reactions exists, use it.
    // Else, try to generate from EventList.

    template <typename S, typename C, bool has_explicit>
    struct Impl;

    template <typename S, typename C>
    struct Impl<S, C, true> {
        using type = typename S::reactions;
    };

    template <typename S, typename C>
    struct Impl<S, C, false> {
        using Events = typename GetEventList<S, C>::type;
        using type = typename AutoReactions<S, Events>::type;
    };

    using type = typename Impl<State, Context, HasReactionsTypedef<State>::value>::type;
};

} // namespace detail
} // namespace ufsm
