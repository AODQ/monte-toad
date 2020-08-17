// -- tonemapping kernel

#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/span.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace {

enum class Strategy : uint8_t {
  None
, Gamma
, Reinhard
, ExtendedReinhard
, Exponential
, Size
};

Strategy strategy;

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
    min = glm::min(min, glm::dot(inputImageBuffer[i], glm::vec3(0.33333f)));
    max = glm::max(max, glm::dot(inputImageBuffer[i], glm::vec3(0.33333f)));
    average += glm::dot(glm::dvec3(inputImageBuffer[i]), glm::dvec3(0.3333333));

    // store value as well
    outputImageBuffer[i] = inputImageBuffer[i];
  }
  average /= static_cast<double>(inputImageBuffer.size());

  // apply tonemapping
  for (auto & value : outputImageBuffer) {
    switch (::strategy) {
      default: break;
      case ::Strategy::None: break;
      case ::Strategy::Gamma:
        value =
          glm::pow(glm::smoothstep(min, max, value), glm::vec3(1.0f/2.2f));
      break;
      case ::Strategy::Reinhard:
        value = value / (value+glm::vec3(1.0f));
      break;
      case ::Strategy::ExtendedReinhard:
        value =
          (value * (glm::vec3(1) + value / (max*max))) / (value + glm::vec3(1));
      break;
      case ::Strategy::Exponential:
        value = 1.0f - exp(-value / max);
        value = glm::pow(value, glm::vec3(1.0f/2.2f));
      break;
    }
    /* auto x = glm::max(glm::vec3(0.0f), value - glm::vec3(0.004f)); */
    /* value = */
    /*     x * (x*6.2f + glm::vec3(0.5f)) */
    /*   / (x * (6.2f*x + glm::vec3(1.7f)) + glm::vec3(0.06f)) */
    /* ; */

    /* value = glm::pow(value, glm::vec3(1.0f/2.2f)); */
  }
}

void UiUpdate(
  mt::core::Scene &
, mt::core::RenderInfo &
, mt::core::IntegratorData & data
, mt::PluginInfo const &
) {
  auto ToStr = [](::Strategy strategy) -> char const * {
    switch (strategy) {
      default: return "N/A";
      case ::Strategy::None:             return "none";
      case ::Strategy::Gamma:            return "gamma";
      case ::Strategy::Reinhard:         return "reinhard";
      case ::Strategy::ExtendedReinhard: return "extended reinhard";
      case ::Strategy::Exponential:      return "exponential";
    }
  };

  if (ImGui::BeginCombo("Strategy", ToStr(::strategy))) {
    for (uint8_t i = 0u; i < Idx(::Strategy::Size); ++ i) {
      auto st = static_cast<::Strategy>(i);
      if (ImGui::Selectable(ToStr(st), Idx(::strategy) == i)) {
        ::strategy = st;

        // will force a preview / all update
        data.renderingFinished = false;
      }
    }
    ImGui::EndCombo();
  }
}

}
