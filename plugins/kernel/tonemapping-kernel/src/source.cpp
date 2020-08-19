// -- tonemapping kernel

#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/span.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace {

// -- tonemapping operators implemented mostly using
//      github.coim/tizian/tonemapper as reference

enum class Strategy : uint8_t {
  AcesNarkowicz
, AcesUnreal
, Amd
, Clamping
, Drago
, Exponential
, Exponentiation
, Ferwerda
, Filmic1
, Filmic2
, GranTurismo
, Insomniac
, Linear
, Logarithmic
, MaxDivision
, MeanValue
, Reinhard
, ReinhardDevlin
, ReinhardExtended
, Schlick
, Srgb
, TumblinRushmeier
, Uncharted
, Ward
, Size
};

struct StrategyInfo {

  struct ImFloatHelper {
    float * value;
    const float min, max;
    char const * label, * description;
  };

  struct AcesNarkowicz {
    char const * label = "ACES (Narkowicz)";
    std::vector<ImFloatHelper> Gui() { return {}; }

    glm::vec3 Tonemap(glm::vec3 const & v) {
      glm::vec3 const
        a = glm::vec3(2.51f)
      , b = glm::vec3(0.03f)
      , c = glm::vec3(2.43f)
      , d = glm::vec3(0.59f)
      , e = glm::vec3(0.14f)
      ;

      return (v*(a*v + b)) / (v*(c*v + d) + e);
    }
  };

  struct AcesUnreal {
    char const * label = "Aces (Unreal)";
    std::vector<ImFloatHelper> Gui() { return {}; }

    glm::vec3 Tonemap(glm::vec3 const & v) {
      return v / (v + glm::vec3(0.155f)) * 1.019f;
    }
  };

  struct Amd {
    char const * label = "Amd";
    float a = 1.6f, d = 0.977f, hdrMax = 8.0f, midIn = 0.18f, midOut = 0.267f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&a, 1.0f, 2.0f, "a", "Contrast"}
      , {&d, 0.01f, 2.0f, "d", "Shoulder"}
      , {&hdrMax, 1.0f, 10.0f, "hdrMax", "hdrMax"}
      , {&midIn, 0.0f, 1.0f, "midIn", "midIn"}
      , {&midOut, 0.0f, 1.0f, "midOut", "midOut"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const & v) {
      glm::vec3 const b =
          glm::vec3(-glm::pow(midIn, a) + glm::pow(hdrMax, a) * midOut)
        / ((glm::pow(hdrMax, a * d) - glm::pow(midIn, a * d)) * midOut)
      ;

      glm::vec3 const c =
        glm::vec3(
          glm::pow(hdrMax, a * d) * glm::pow(midIn, a)
        - glm::pow(hdrMax, a) * glm::pow(midIn, a * d) * midOut
        )
      / ((glm::pow(hdrMax, a * d) - glm::pow(midIn, a * d)) * midOut)
      ;

      return
          glm::pow(v, glm::vec3(a))
        / (glm::pow(v, glm::vec3(a * d)) * b + glm::vec3(c))
      ;
    }
  };

  struct Clamping {
    char const * label = "Clamping";

    std::vector<ImFloatHelper> Gui() { return {}; }

    glm::vec3 Tonemap(glm::vec3 v, glm::vec3 Lb, glm::vec3 Lw) {
      return mix(Lb, Lw, glm::vec3(0.5f)) / v;
    }
  };

  struct Drago {
    char const * label = "Drago";
    float slope = 4.5f, start = 0.018f, ldMax = 100.0f, bias = 0.85f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&slope, 0.0f, 10.0f, "slope", "gamma correction slope"}
      , {&start, 0.0f, 2.0f, "start", "abscissa at point of tangency"}
      , {&ldMax, 0.0f, 200.0f, "ldMax", "max luminance capability of display"}
      , {&bias, 0.0f, 1.0f, "bias", "bias"}
      };
    }

    // TODO this has a gamma correct

    glm::vec3 Tonemap(glm::vec3 const &) {
      // TODO
      return glm::vec3(0);
    }
  };

  struct Exponential {
    char const * label = "Exponential";
    float p = 1.0f, q = 1.0f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&p, 0.0f, 20.0f, "p", "numerator scale"}
      , {&q, 0.0f, 20.0f, "q", "denominator scale"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 v, glm::vec3 /*lW*/, glm::vec3 lAvg) {
      return glm::vec3(1.0f) - glm::exp(-(v*p) / (lAvg * q));
    }
  };

  struct Exponentiation {
    char const * label = "Exponentiation";
    float curve = 0.5f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&curve, 0.0f, 1.0f, "p", "curve exponent"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const & v, glm::vec3 lMax) {
      // TODO this and some others need exposure setting
      return v / lMax;
    }
  };

  struct Ferwerda {
    char const * label = "Ferwerda";
    float ldMax = 80.0f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&ldMax, 0.0f, 160.0f, "ldMax", "max luminance capability of display"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const &) {
      // TODO this one is weird
      return glm::vec3(1.0f);
    }
  };

  struct Filmic1 {
    char const * label = "Filmic1";

    std::vector<ImFloatHelper> Gui() { return {}; }

    glm::vec3 Tonemap(glm::vec3 v) {
      v = glm::max(glm::vec3(0.0f), v - glm::vec3(0.004f));
      return
        v * (6.2f*v + glm::vec3(0.5f))
      / (v * (6.2f*v + glm::vec3(1.7f) + glm::vec3(0.06f)))
      ;
    }
  };

  struct Filmic2 {
    char const * label = "Filmic2";
    float cutoff = 0.025f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&cutoff, 0.0f, 0.5f, "cutoff", "transition into compressed blacks"}
      };
    }
    glm::vec3 Tonemap(glm::vec3 v) {
      v +=
          glm::vec3(cutoff*2.0f - v)
        + glm::vec3(
            clamp(cutoff*2.0f - v, 0.0f, 1.0f) * (0.25f/cutoff) - cutoff
          )
      ;

      return
          v * (6.2f*v + glm::vec3(0.5f))
        / (v*(6.2f*v + glm::vec3(1.7f)) + glm::vec3(0.06f))
      ;
    }
  };

  struct GranTurismo {
    char const * label = "GranTurismo";
    float p = 1.0f, a = 1.0f, m = 0.22f, l = 0.4f, c = 1.33f, b = 0.0f ;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&p, 1.0f, 100.0f, "p", "maximum brightness"}
      , {&a, 0.0f, 5.0f, "a", "contrast"}
      , {&m, 0.0f, 1.0f, "m", "linear section start"}
      , {&l, 0.01f, 0.99f, "l", "linear section length"}
      , {&c, 1.0f, 3.0f, "c", "black tightness-c"}
      , {&b, 0.0f, 1.0f, "b", "black tightness-b"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const &) {
      // TODO again lol
      return glm::vec3(0.0f);
    }
  };

  struct Insomniac {
    char const * label = "Insomniac";
    float w = 10.0f, b = 0.1f, t = 0.7f, s = 0.8f, c = 2.0f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&w, 0.0f, 20.0f, "w", "white point"}
      , {&b, 0.0f, 2.0f, "b", "black point"}
      , {&t, 0.0f, 1.0f, "t", "toe strength"}
      , {&s, 0.0f, 1.0f, "s", "shoulder strength"}
      , {&c, 0.0f, 10.0f, "c", "cross-over point"}
      };
    }
    glm::vec3 Tonemap(glm::vec3 const &) {
      // TODO
      return glm::vec3(1.0f);
    }
  };

  struct Linear {
    char const * label = "Linear";

    std::vector<ImFloatHelper> Gui() { return {}; }
    glm::vec3 Tonemap(glm::vec3 const & v) {
      return glm::clamp(v, 0.0f, 1.0f);
    }
  };

  struct Logarithmic {
    char const * label = "Logarithmic";
    float p = 1.0f, q = 1.0f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&p, 0.0f, 20.0f, "p", "numerator scale"}
      , {&q, 0.0f, 20.0f, "q", "denominator scale"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const) {
      // TODO no log10
      return glm::vec3(1.0f);
          /* glm::log10(glm::vec3(1.0f) + v*p) */
        /* / glm::log10(glm::vec3(1.0f) + q*wP*q) */
      /* ; */
    }
  };

  struct MaxDivision {
    char const * label = "MaxDivision";

    std::vector<ImFloatHelper> Gui() { return {}; }

    glm::vec3 Tonemap(glm::vec3 const & v, glm::vec3 const & wP) {
      return v / wP;
    }
  };

  struct MeanValue {
    char const * label = "MeanValue";

    std::vector<ImFloatHelper> Gui() { return {}; }

    glm::vec3 Tonemap(glm::vec3 const & v, glm::vec3 wP) {
      return glm::vec3(0.5f) * v/wP;
    }
  };

  struct Reinhard {
    char const * label = "Reinhard";

    std::vector<ImFloatHelper> Gui() { return {}; }

    glm::vec3 Tonemap(glm::vec3 const & v) {
      return v/(glm::vec3(1.0f)+v);
    }
  };

  struct ReinhardDevlin {
    char const * label = "ReinhardDevlin";
    float m = 0.5f, f = 1.0f, c = 0.0f, a = 1.0f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&m, 0.0f, 1.0f, "m", "compression curve adjustment"}
      , {&f, 0.0f, 1000.0f, "f", "intensity adjustment"}
      , {&c, 0.0f, 1.0f, "c", "chromatic adaptation (color <-> luminance)"}
      , {&a, 0.0f, 1.0f, "a", "light adaptation (pixel+scene mix intensity)"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const &) {
      // TODO lol
      return glm::vec3(0.0f);
    }
  };

  struct ReinhardExtended {
    char const * label = "ReinhardExtended";

    std::vector<ImFloatHelper> Gui() { return {}; }

    glm::vec3 Tonemap(glm::vec3 const & v, glm::vec3 lWhite) {
      return v*(glm::vec3(1.0f) + v/(lWhite*lWhite)) / (glm::vec3(1.0f) + v);
    }
  };

  struct Schlick {
    char const * label = "Schlick";
    float p = 200.0f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&p, 1.0f, 1000.0f, "p", "rational mapping curve"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const &) {
      // TODO
      return glm::vec3(1.0f);
    }
  };

  struct Srgb {
    char const * label = "Srgb";

    std::vector<ImFloatHelper> Gui() { return {}; }

    glm::vec3 Tonemap(glm::vec3 v) {
      v.r =
        v.r < 0.0031308f ? 12.92f*v.r : 1.055f*glm::pow(v.r, 0.41666f) - 0.055f;
      v.g =
        v.g < 0.0031308f ? 12.92f*v.g : 1.055f*glm::pow(v.g, 0.41666f) - 0.055f;
      v.b =
        v.b < 0.0031308f ? 12.92f*v.b : 1.055f*glm::pow(v.b, 0.41666f) - 0.055f;
      return v;
    }
  };

  struct TumblinRushmeier {
    char const * label = "TumblinRushmeier";
    float ldMax = 86.0f, cMax = 50.0f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&ldMax, 1.0f, 200.0f, "Ldmax", "max luminance capability of display"}
      , {&cMax, 1.0f, 500.0f, "Cmax", "max contrast ratio between luminances"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const &) {
      // TODO
      return glm::vec3(1.0f);
    }
  };

  struct Uncharted {
    char const * label = "Uncharted";

    float
      a = 0.22f, b = 0.3f, c = 0.1f, d = 0.2f, e = 0.01f, f = 0.3f, w = 11.2f;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&a, 0.0f, 1.0f, "a", "shoulder strength"}
      , {&b, 0.0f, 1.0f, "b", "linear strength"}
      , {&c, 0.0f, 1.0f, "c", "linear angle"}
      , {&d, 0.0f, 1.0f, "d", "toe strength"}
      , {&e, 0.0f, 1.0f, "e", "toe numerator"}
      , {&f, 0.0f, 1.0f, "f", "toe denominator"}
      , {&w, 0.0f, 20.0f, "w", "white point"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const &) {
      // TODO
      return glm::vec3(1.0f);
    }
  };

  struct Ward {
    char const * label = "Ward";
    float ldMax;

    std::vector<ImFloatHelper> Gui() {
      return {
        {&ldMax, 0.0f, 200.0f, "Ldmax", "max luminance capability of display"}
      };
    }

    glm::vec3 Tonemap(glm::vec3 const &) {
      return glm::vec3(1.0f);
    }
  };

  union {
    Linear linear; // default

    AcesNarkowicz acesNarkowicz;
    AcesUnreal acesUnreal;
    Amd amd;
    Clamping clamping;
    Drago drago;
    Exponential exponential;
    Exponentiation exponentiation;
    Ferwerda ferwerda;
    Filmic1 filmic1;
    Filmic2 filmic2;
    GranTurismo granTurismo;
    Insomniac insomniac;
    Logarithmic logarithmic;
    MaxDivision maxDivision;
    MeanValue meanValue;
    Reinhard reinhard;
    ReinhardDevlin reinhardDevlin;
    ReinhardExtended reinhardExtended;
    Schlick schlick;
    Srgb srgb;
    TumblinRushmeier tumblinRushmeier;
    Uncharted uncharted;
    Ward ward;
  };

  Strategy strategy;
  float exposure;
  float gamma;
};

StrategyInfo strategyInfo {
  StrategyInfo::Linear{}, Strategy::Linear, 1.0f, 2.2f
};

char const * ToString(Strategy s) {
  switch (s) {
    case Strategy::Size: return "N/A";
    case Strategy::AcesNarkowicz:    return "Aces Narkowicz";
    case Strategy::AcesUnreal:       return "Aces Unreal";
    case Strategy::Amd:              return "Amd";
    case Strategy::Clamping:         return "Clamping";
    case Strategy::Drago:            return "Drago";
    case Strategy::Exponential:      return "Exponential";
    case Strategy::Exponentiation:   return "Exponentiation";
    case Strategy::Ferwerda:         return "Ferwerda";
    case Strategy::Filmic1:          return "Filmic1";
    case Strategy::Filmic2:          return "Filmic2";
    case Strategy::GranTurismo:      return "Gran-Turismo";
    case Strategy::Insomniac:        return "Insomniac";
    case Strategy::Linear:           return "Linear";
    case Strategy::Logarithmic:      return "Logarithmic";
    case Strategy::MaxDivision:      return "Max-Division";
    case Strategy::MeanValue:        return "Mean-Value";
    case Strategy::Reinhard:         return "Reinhard";
    case Strategy::ReinhardDevlin:   return "Reinhard-Devlin";
    case Strategy::ReinhardExtended: return "Reinhard-Extended";
    case Strategy::Schlick:          return "Schlick";
    case Strategy::Srgb:             return "Srgb";
    case Strategy::TumblinRushmeier: return "Tumblin-Rushmeier";
    case Strategy::Uncharted:        return "Uncharted";
    case Strategy::Ward:             return "Ward";
  }
  return "N/A";
}

} // -- anon namespace

extern "C" {

char const * PluginLabel() { return "tonemapping kernel"; }
mt::PluginType PluginType() { return mt::PluginType::Kernel; }

void ApplyKernel(
  mt::core::RenderInfo & /*render*/
, mt::PluginInfo const & /*plugin*/
, mt::core::IntegratorData & /*integratorData*/
, span<glm::vec3> inputImageBuffer // TODO make this const
, span<glm::vec3> outputImageBuffer
) {

  // -- compute average tonemapping variables
  float
    min = +std::numeric_limits<float>::max()
  , max = -std::numeric_limits<float>::max()
  ;

  double average = 0.0;

  for (size_t i = 0ul; i < inputImageBuffer.size(); ++ i) {
    auto value = ::strategyInfo.exposure * inputImageBuffer[i];
    min = glm::min(min, glm::dot(value, glm::vec3(0.33333f)));
    max = glm::max(max, glm::dot(value, glm::vec3(0.33333f)));
    average += glm::dot(glm::dvec3(value), glm::dvec3(0.3333333));

    // store value as well
    outputImageBuffer[i] = value;
  }
  average /= static_cast<double>(inputImageBuffer.size());

  // apply tonemap + gamma correction
  for (auto & value : outputImageBuffer) {

    // -- tonemapping
    switch (::strategyInfo.strategy) {
      default: break;
      case ::Strategy::AcesNarkowicz:
        value = ::strategyInfo.acesNarkowicz.Tonemap(value);
      break;
      case ::Strategy::AcesUnreal:
        value = ::strategyInfo.acesUnreal.Tonemap(value);
      break;
      case ::Strategy::Amd:
        value = ::strategyInfo.amd.Tonemap(value);
      break;
      case ::Strategy::Clamping:
        value =
          ::strategyInfo.clamping.Tonemap(value, glm::vec3(min), glm::vec3(max))
        ;
      break;
      case ::Strategy::Drago:
        value = ::strategyInfo.drago.Tonemap(value);
      break;
      case ::Strategy::Exponential:
        value =
          ::strategyInfo.exponential.Tonemap(
            value, glm::vec3(max), glm::vec3(average)
          );
      break;
      case ::Strategy::Exponentiation:
        value = ::strategyInfo.exponentiation.Tonemap(value, glm::vec3(max));
      break;
      case ::Strategy::Ferwerda:
        value = ::strategyInfo.ferwerda.Tonemap(value);
      break;
      case ::Strategy::Filmic1:
        value = ::strategyInfo.filmic1.Tonemap(value);
      break;
      case ::Strategy::Filmic2:
        value = ::strategyInfo.filmic2.Tonemap(value);
      break;
      case ::Strategy::GranTurismo:
        value = ::strategyInfo.granTurismo.Tonemap(value);
      break;
      case ::Strategy::Insomniac:
        value = ::strategyInfo.insomniac.Tonemap(value);
      break;
      case ::Strategy::Linear:
        value = ::strategyInfo.linear.Tonemap(value);
      break;
      case ::Strategy::Logarithmic:
        value = ::strategyInfo.logarithmic.Tonemap(value);
      break;
      case ::Strategy::MaxDivision:
        value = ::strategyInfo.maxDivision.Tonemap(value, glm::vec3(max));
      break;
      case ::Strategy::MeanValue:
        value = ::strategyInfo.meanValue.Tonemap(value, glm::vec3(max));
      break;
      case ::Strategy::Reinhard:
        value = ::strategyInfo.reinhard.Tonemap(value);
      break;
      case ::Strategy::ReinhardDevlin:
        value = ::strategyInfo.reinhardDevlin.Tonemap(value);
      break;
      case ::Strategy::ReinhardExtended:
        value = ::strategyInfo.reinhardExtended.Tonemap(value, glm::vec3(max));
      break;
      case ::Strategy::Schlick:
        value = ::strategyInfo.schlick.Tonemap(value);
      break;
      case ::Strategy::Srgb:
        value = ::strategyInfo.srgb.Tonemap(value);
      break;
      case ::Strategy::TumblinRushmeier:
        value = ::strategyInfo.tumblinRushmeier.Tonemap(value);
      break;
      case ::Strategy::Uncharted:
        value = ::strategyInfo.uncharted.Tonemap(value);
      break;
      case ::Strategy::Ward:
        value = ::strategyInfo.ward.Tonemap(value);
      break;
    }

    // -- gamma
    value =
      glm::pow(
        glm::clamp(glm::vec3(value), glm::vec3(0.0f), glm::vec3(1.0f))
      , glm::vec3(1.0f/::strategyInfo.gamma)
      );
  }
}

void UiUpdate(
  mt::core::Scene &
, mt::core::RenderInfo &
, mt::core::IntegratorData & data
, mt::PluginInfo const &
) {
  bool hasChanged = false;
  if (ImGui::BeginCombo("Strategy", ::ToString(::strategyInfo.strategy))) {
    for (uint8_t i = 0u; i < Idx(::Strategy::Size); ++ i) {
      auto st = static_cast<::Strategy>(i);
      if (ImGui::Selectable(ToString(st), Idx(::strategyInfo.strategy) == i)) {
        ::strategyInfo.strategy = st;
        hasChanged = true;

        // will force a preview / all update
        data.renderingFinished = false;
      }
    }
    ImGui::EndCombo();
  }

  if (ImGui::Button("<")) {
    hasChanged = true;
    ::strategyInfo.strategy =
      static_cast<::Strategy>(
        (
          static_cast<size_t>(::strategyInfo.strategy)
        + static_cast<size_t>(::Strategy::Size) - 1ul
        )
      % static_cast<size_t>(::Strategy::Size)
      );
  }

  ImGui::SameLine();
  if (ImGui::Button(">")) {
    hasChanged = true;
    ::strategyInfo.strategy =
      static_cast<::Strategy>(
        (static_cast<size_t>(::strategyInfo.strategy) + 1ul)
      % static_cast<size_t>(::Strategy::Size)
      );
  }

  if (hasChanged) {
    using S = ::Strategy;
    auto & s = ::strategyInfo;
    switch (::strategyInfo.strategy) {
      case S::Size: break;
      case S::AcesNarkowicz:    s.acesNarkowicz    = {}; break;
      case S::AcesUnreal:       s.acesUnreal       = {}; break;
      case S::Amd:              s.amd              = {}; break;
      case S::Clamping:         s.clamping         = {}; break;
      case S::Drago:            s.drago            = {}; break;
      case S::Exponential:      s.exponential      = {}; break;
      case S::Exponentiation:   s.exponentiation   = {}; break;
      case S::Ferwerda:         s.ferwerda         = {}; break;
      case S::Filmic1:          s.filmic1          = {}; break;
      case S::Filmic2:          s.filmic2          = {}; break;
      case S::GranTurismo:      s.granTurismo      = {}; break;
      case S::Insomniac:        s.insomniac        = {}; break;
      case S::Linear:           s.linear           = {}; break;
      case S::Logarithmic:      s.logarithmic      = {}; break;
      case S::MaxDivision:      s.maxDivision      = {}; break;
      case S::MeanValue:        s.meanValue        = {}; break;
      case S::Reinhard:         s.reinhard         = {}; break;
      case S::ReinhardDevlin:   s.reinhardDevlin   = {}; break;
      case S::ReinhardExtended: s.reinhardExtended = {}; break;
      case S::Schlick:          s.schlick          = {}; break;
      case S::Srgb:             s.srgb             = {}; break;
      case S::TumblinRushmeier: s.tumblinRushmeier = {}; break;
      case S::Uncharted:        s.uncharted        = {}; break;
      case S::Ward:             s.ward             = {}; break;
    }
  }

  if (ImGui::SliderFloat("exposure", &::strategyInfo.exposure, 0.0f, 5.0f)) {
    data.renderingFinished = false;
  }
  if (ImGui::SliderFloat("gamma", &::strategyInfo.gamma, 1.0f, 3.0f)) {
    data.renderingFinished = false;
  }

  auto info = []() -> std::vector<::StrategyInfo::ImFloatHelper> {
    using S = ::Strategy;
    auto & s = ::strategyInfo;
    switch (::strategyInfo.strategy) {
      case S::Size: return {};
      case S::AcesNarkowicz:    return s.acesNarkowicz.Gui();
      case S::AcesUnreal:       return s.acesUnreal.Gui();
      case S::Amd:              return s.amd.Gui();
      case S::Clamping:         return ::strategyInfo.clamping.Gui();
      case S::Drago:            return ::strategyInfo.drago.Gui();
      case S::Exponential:      return ::strategyInfo.exponential.Gui();
      case S::Exponentiation:   return ::strategyInfo.exponentiation.Gui();
      case S::Ferwerda:         return ::strategyInfo.ferwerda.Gui();
      case S::Filmic1:          return ::strategyInfo.filmic1.Gui();
      case S::Filmic2:          return ::strategyInfo.filmic2.Gui();
      case S::GranTurismo:      return ::strategyInfo.granTurismo.Gui();
      case S::Insomniac:        return ::strategyInfo.insomniac.Gui();
      case S::Linear:           return ::strategyInfo.linear.Gui();
      case S::Logarithmic:      return ::strategyInfo.logarithmic.Gui();
      case S::MaxDivision:      return ::strategyInfo.maxDivision.Gui();
      case S::MeanValue:        return ::strategyInfo.meanValue.Gui();
      case S::Reinhard:         return ::strategyInfo.reinhard.Gui();
      case S::ReinhardDevlin:   return ::strategyInfo.reinhardDevlin.Gui();
      case S::ReinhardExtended: return ::strategyInfo.reinhardExtended.Gui();
      case S::Schlick:          return ::strategyInfo.schlick.Gui();
      case S::Srgb:             return ::strategyInfo.srgb.Gui();
      case S::TumblinRushmeier: return ::strategyInfo.tumblinRushmeier.Gui();
      case S::Uncharted:        return ::strategyInfo.uncharted.Gui();
      case S::Ward:             return ::strategyInfo.ward.Gui();
    }

    return {};
  }();

  for (auto & helper : info) {
    if (ImGui::SliderFloat(helper.label, helper.value, helper.min, helper.max))
      { data.renderingFinished = false; }
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("%s", helper.description);
      ImGui::EndTooltip();
    }
  }
}

}
