#pragma once

#include <ufsm/type/list.h>

namespace ufsm {
namespace detail {

template <class ContextList, class OutermostContextBase>
struct Constructor;

template <class ContextList, class OutermostContextBase>
struct OuterConstructor {
  typedef typename mp::Front<ContextList>::type ToConstruct;
  typedef typename ToConstruct::ContextPtrType ContextPtrType;
  typedef typename ToConstruct::InnerContextPtrType InnerContextPtrType;

  typedef typename mp::PopFront<ContextList>::type InnerContextList;

  static void Construct(const ContextPtrType& p_context,
                        OutermostContextBase& outermost_context_base) {
    const InnerContextPtrType p_inner_context =
        ToConstruct::ShallowConstruct(p_context, outermost_context_base);
    Constructor<InnerContextList, OutermostContextBase>::Construct(
        p_inner_context, outermost_context_base);
  }
};

template <class ContextList, class OutermostContextBase>
struct InnerConstructor {
  typedef typename mp::Front<ContextList>::type ToConstruct;
  typedef typename ToConstruct::ContextPtrType ContextPtrType;

  static void Construct(const ContextPtrType& p_context,
                        OutermostContextBase& outermost_context_base) {
    ToConstruct::DeepConstruct(p_context, outermost_context_base);
  }
};

template <class ContextList, class OutermostContextBase>
struct ConstructorImpl
    : public mp::If<mp::Size<ContextList>::value == 1,
                    InnerConstructor<ContextList, OutermostContextBase>,
                    OuterConstructor<ContextList, OutermostContextBase> > {};

template <class ContextList, class OutermostContextBase>
struct Constructor : ConstructorImpl<ContextList, OutermostContextBase>::type {
};

template <class CommonContext, class DestinationState>
struct MakeContextList {
  using type = typename mp::Reverse<typename mp::PushFront<
      typename mp::Range<typename DestinationState::ContextTypeList, 0,
                         mp::Find<typename DestinationState::ContextTypeList,
                                  CommonContext>::value>::type,
      DestinationState>::type>::type;
};

}  // namespace detail
}  // namespace ufsm
