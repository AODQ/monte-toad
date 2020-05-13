#include <type_traits>

template <typename EnumType> constexpr auto Idx(EnumType const & v) {
  return static_cast<std::underlying_type<EnumType>::type>(v);
}

template <typename EnumType>
constexpr std::underlying_type<EnumType>::type & Idx(EnumType & v) {
  return reinterpret_cast<std::underlying_type<EnumType>::type &>(v);
}
