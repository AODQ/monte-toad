#include <monte-toad/enum.hpp>

#include <spdlog/spdlog.h>

#include <array>   // for std::array
#include <ctype.h> // for ::tolower
#include <string>  // for std::string

mt::RenderingState mt::ToRenderingState(char const * label) {
  auto fixLabel = std::string{label};
  for (auto & c : fixLabel) { c = ::tolower(c); }

  if (fixLabel == "off")          { return mt::RenderingState::Off;         }
  if (fixLabel == "on-change")    { return mt::RenderingState::OnChange;    }
  if (fixLabel == "after-change") { return mt::RenderingState::AfterChange; }
  if (fixLabel == "onalways")     { return mt::RenderingState::OnAlways;    }

  spdlog::error("unknown rendering state '{}', defaulting to off", label);

  return mt::RenderingState::Off;
}

mt::AspectRatio mt::ToAspectRatio(char const * label) {
  auto fixLabel = std::string{label};
  for (auto & c : fixLabel) { c = ::tolower(c); }

  if (fixLabel == "1x1")   { return mt::AspectRatio::e1_1; }
  if (fixLabel == "3x2")   { return mt::AspectRatio::e3_2; }
  if (fixLabel == "4x3")   { return mt::AspectRatio::e4_3; }
  if (fixLabel == "5x4")   { return mt::AspectRatio::e5_4; }
  if (fixLabel == "16x9")  { return mt::AspectRatio::e16_9; }
  if (fixLabel == "16x10") { return mt::AspectRatio::e16_10; }
  if (fixLabel == "21x9")  { return mt::AspectRatio::e21_9; }
  if (fixLabel == "eNone")  { return mt::AspectRatio::eNone; }

  spdlog::error("unknown aspect ratio '{}', defaulting to 4x3", label);

  return mt::AspectRatio::e4_3;
}

void mt::ApplyAspectRatioY(
  mt::AspectRatio ratio
, uint16_t const x, uint16_t & y
) {
  std::array<float, Idx(mt::AspectRatio::size)> constexpr
    aspectRatioConversion = {{
      1.0f, 3.0f/2.0f, 4.0f/3.0f, 5.0f/4.0f, 16.0f/9.0f, 16.0f/10.0f, 21.0f/9.0f
    , 1.0f
    }};

  if (ratio == mt::AspectRatio::eNone) { return; }

  y = x / aspectRatioConversion[Idx(ratio)];
  if (y == 0) { y = 1; }
}

char const * mt::ToString(mt::IntegratorTypeHint hint) {
  switch (hint) {
    default: return "";
    case mt::IntegratorTypeHint::Primary: return "Primary";
    case mt::IntegratorTypeHint::Albedo:  return "Albedo";
    case mt::IntegratorTypeHint::Normal:  return "Normal";
    case mt::IntegratorTypeHint::Depth:   return "Depth";
  }
}
