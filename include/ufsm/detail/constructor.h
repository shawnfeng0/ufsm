#pragma once

#include <ufsm/type/list.h>

namespace ufsm {
namespace detail {

template <class ContextList, class OutermostContextBase>
struct Constructor {
  template <typename ContextPtrType>
  static void Construct(const ContextPtrType& p_context, OutermostContextBase& outermost_context_base) {
    using ToConstruct = typename mp::Front<ContextList>::type;

    if constexpr (mp::Size<ContextList>::value == 1) {
      ToConstruct::DeepConstruct(p_context, outermost_context_base);
    } else {
      using InnerContextPtrType = typename ToConstruct::InnerContextPtrType;
      using InnerContextList = typename mp::PopFront<ContextList>::type;

      const InnerContextPtrType p_inner_context = ToConstruct::ShallowConstruct(p_context, outermost_context_base);
      Constructor<InnerContextList, OutermostContextBase>::Construct(p_inner_context, outermost_context_base);
    }
  }
};

template <class CommonContext, class DestinationState>
struct MakeContextList {
  using type = typename mp::Reverse<typename mp::PushFront<
      typename mp::Range<typename DestinationState::ContextTypeList, 0,
                         mp::Find<typename DestinationState::ContextTypeList, CommonContext>::value>::type,
      DestinationState>::type>::type;
};

}  // namespace detail
}  // namespace ufsm
