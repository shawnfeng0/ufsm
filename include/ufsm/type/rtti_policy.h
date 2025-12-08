#pragma once

namespace ufsm {
namespace detail {

// Each type gets a unique address via static member
template <typename MostDerived>
struct IdHolder {
  static inline char id_tag_;  // C++17 inline variable
};

struct RttiPolicy {
  using IdType = const void*;

  class RttiBaseType {
   public:
    IdType dynamic_type() const noexcept { return id_; }

   protected:
    explicit RttiBaseType(IdType id) : id_(id) {}
    ~RttiBaseType() = default;

   private:
    IdType id_;
  };

  template <typename MostDerived, typename Base>
  class RttiDerivedType : public Base {
   public:
    static IdType static_type() noexcept { return &IdHolder<MostDerived>::id_tag_; }

   protected:
    ~RttiDerivedType() = default;
    RttiDerivedType() : Base(&IdHolder<MostDerived>::id_tag_) {}
  };
};

}  // namespace detail
}  // namespace ufsm
