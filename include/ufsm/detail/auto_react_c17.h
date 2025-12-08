#pragma once

#include <ufsm/reaction.h>
#include <ufsm/type/list.h>

#include <type_traits>
#include <utility>

namespace ufsm {
namespace detail {

template <typename T, typename E, typename = void>
struct HasReactMethod : std::false_type {};

template <typename T, typename E>
struct HasReactMethod<T, E, ufsm::void_t<decltype(std::declval<T>().React(std::declval<const E&>()))>>
    : std::true_type {};

template <typename E>
struct MakeReaction {
  using type = Reaction<E>;
};

template <typename State, typename EventList>
struct AutoReactions {
  template <typename E>
  using Predicate = HasReactMethod<State, E>;

  using FilteredEvents = typename mp::Filter<EventList, Predicate>::type;
  using type = typename mp::Transform<FilteredEvents, MakeReaction>::type;
};

struct AutoReactionsTag {};

// --- EventList Trait for breaking circular dependencies ---
}  // namespace detail

template <typename T>
struct EventListTrait {
  using type = void;
};

namespace detail {

// --- Helper to get reactions or fallback ---

template <typename T, typename = void>
struct HasReactionsTypedef : std::false_type {};

template <typename T>
struct HasReactionsTypedef<T, ufsm::void_t<typename T::reactions>> : std::true_type {};

template <typename T, typename = void>
struct HasEventListTypedef : std::false_type {};

template <typename T>
struct HasEventListTypedef<T, ufsm::void_t<typename T::EventList>> : std::true_type {};

// Helper to find EventList in Context (recursively?)
// For now, let's look in MostDerived::EventList, then Context::EventList.

template <typename State, typename Context, typename = void>
struct GetEventList {
  using type = mp::List<>;  // Default empty
};

// Priority 1: EventListTrait<Context>
template <typename State, typename Context>
struct GetEventList<
    State, Context,
    typename std::enable_if<!std::is_same<typename ufsm::EventListTrait<Context>::type, void>::value>::type> {
  using type = typename ufsm::EventListTrait<Context>::type;
};

// Priority 2: State::EventList
template <typename State, typename Context>
struct GetEventList<State, Context,
                    typename std::enable_if<std::is_same<typename ufsm::EventListTrait<Context>::type, void>::value &&
                                            HasEventListTypedef<State>::value>::type> {
  using type = typename State::EventList;
};

// Priority 3: Context::EventList
template <typename State, typename Context>
struct GetEventList<
    State, Context,
    typename std::enable_if<std::is_same<typename ufsm::EventListTrait<Context>::type, void>::value &&
                            !HasEventListTypedef<State>::value && HasEventListTypedef<Context>::value>::type> {
  using type = typename Context::EventList;
};

template <typename State, typename Context>
struct GetReactions {
  using type = mp::If<HasReactionsTypedef<State>::value, typename State::reactions,
                      typename AutoReactions<State, typename GetEventList<State, Context>::type>::type>;
};

}  // namespace detail
}  // namespace ufsm
