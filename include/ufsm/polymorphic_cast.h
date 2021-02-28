#pragma once

#include <cassert>
#include <exception>
#include <type_traits>

// From boost
namespace ufsm {
//  See the documentation for descriptions of how to choose between
//  static_cast<>, dynamic_cast<>, polymorphic_cast<> and polymorphic_downcast<>

//  polymorphic_cast  --------------------------------------------------------//

//  Runtime checked polymorphic downcasts and crosscasts.
//  Suggested in The C++ Programming Language, 3rd Ed, Bjarne Stroustrup,
//  section 15.8 exercise 1, page 425.

template <class Target, class Source>
inline Target polymorphic_cast(Source* x) {
  auto tmp = dynamic_cast<Target>(x);
  if (tmp == 0) throw std::bad_cast();
  return tmp;
}

//  polymorphic_downcast  ----------------------------------------------------//

//  BOOST_ASSERT() checked raw pointer polymorphic downcast.  Crosscasts
//  prohibited.

//  WARNING: Because this cast uses BOOST_ASSERT(), it violates
//  the One Definition Rule if used in multiple translation units
//  where BOOST_DISABLE_ASSERTS, BOOST_ENABLE_ASSERT_HANDLER
//  NDEBUG are defined inconsistently.

//  Contributed by Dave Abrahams

template <class Target, class Source>
inline Target polymorphic_downcast(Source* x) {
  assert(dynamic_cast<Target>(x) == x);  // detect logic error
  return static_cast<Target>(x);
}

//  BOOST_ASSERT() checked reference polymorphic downcast.  Crosscasts
//  prohibited.

//  WARNING: Because this cast uses BOOST_ASSERT(), it violates
//  the One Definition Rule if used in multiple translation units
//  where BOOST_DISABLE_ASSERTS, BOOST_ENABLE_ASSERT_HANDLER
//  NDEBUG are defined inconsistently.

//  Contributed by Julien Delacroix

template <class Target, class Source>
inline typename std::enable_if<std::is_reference<Target>::value, Target>::type
polymorphic_downcast(Source& x) {
  typedef typename std::remove_reference<Target>::type* target_pointer_type;
  return *polymorphic_downcast<target_pointer_type>(std::addressof(x));
}

}  // namespace ufsm
