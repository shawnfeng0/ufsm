#pragma once

namespace ufsm {
namespace detail {

struct IdProvider {
  const void* p_custom_id_;
};

template <class MostDerived>
struct IdHolder {
  static IdProvider id_provider_;
};

template <class MostDerived>
IdProvider IdHolder<MostDerived>::id_provider_;

struct RttiPolicy {
  typedef const void* IdType;
  typedef const IdProvider* IdProviderType;

  class RttiBaseType {
   public:
    typedef RttiPolicy::IdType IdType;

    IdType dynamic_type() const { return id_provider_; }

   protected:
    explicit RttiBaseType(IdProviderType id_provider)
        : id_provider_(id_provider) {}

    ~RttiBaseType() = default;

   private:
    IdProviderType id_provider_;
  };

  template <class MostDerived, class Base>
  class RttiDerivedType : public Base {
   public:
    static IdType static_type() { return &IdHolder<MostDerived>::id_provider_; }

   protected:
    ////////////////////////////////////////////////////////////////////////
    ~RttiDerivedType() = default;
    RttiDerivedType() : Base(&IdHolder<MostDerived>::id_provider_) {}
  };
};

}  // namespace detail
}  // namespace ufsm
