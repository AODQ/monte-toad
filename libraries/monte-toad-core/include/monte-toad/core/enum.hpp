#pragma once

#include <cstdint>

// -- useful functions to eliminate need of static_cast

template <typename EnumType> constexpr auto Idx(EnumType const & v) {
  return static_cast<typename std::underlying_type<EnumType>::type>(v);
}

template <typename EnumType>
constexpr typename std::underlying_type<EnumType>::type & Idx(EnumType & v) {
  return reinterpret_cast<typename std::underlying_type<EnumType>::type &>(v);
}

// not part of core namespace since they're just enums
namespace mt {
  enum struct CullFace : uint8_t { None, Front, Back };

  enum struct RenderingState : uint8_t {
    Off      // never renders
  , OnChange // renders up to N samples only when something has changed
  , AfterChange  // Same as on change but only happens after movement is done
  , OnAlways  // renders only 1 sample all the time
  , Size
  };

  enum struct IntegratorTypeHint : uint8_t {
    Primary
  , Albedo
  , Normal
  , Depth
  , Size
  };

  enum struct BsdfTypeHint : uint8_t {
    Diffuse
  , Specular
  , Refractive
  , Reflective
  };

  enum struct AspectRatio : uint8_t {
    e1_1, e3_2, e4_3, e5_4, e16_9, e16_10, e21_9, eNone, size
  };

  // TODO probably move out? this is more internal and not ui related
  // Indicates how the ray was/should-be cast; either generated naturally for
  // Radiance, such as from the BSDF or generated from importance-sampled
  // information, such as next-event estimation or BDPT etc
  enum TransportMode : uint8_t { Radiance, Importance };

  RenderingState ToRenderingState(char const * label);
  AspectRatio ToAspectRatio(char const * label);
  void ApplyAspectRatioY(mt::AspectRatio ratio, uint16_t const x, uint16_t & y);

  char const * ToString(mt::IntegratorTypeHint hint);
}
