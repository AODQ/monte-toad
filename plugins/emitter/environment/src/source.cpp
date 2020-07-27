// environmental emitter

#include <monte-toad/core/geometry.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <monte-toad/core/texture.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace {

static float emissionPower = 1.0f;

glm::vec3 SampleEmission(
  mt::core::Scene const & /*scene*/
, mt::core::Texture const & texture
, glm::vec3 wo
) {
  return mt::core::Sample(texture, wo);
}

} // -- namespace

extern "C" {

char const * PluginLabel() { return "environment emitter"; }
mt::PluginType PluginType() { return mt::PluginType::Emitter; }

mt::PixelInfo SampleLi(
  mt::core::Scene const & scene
, mt::PluginInfo const & plugin
, mt::core::SurfaceInfo const & surface
, glm::vec3 & wo
, float & pdf
) {
  if (!scene.emissionSource.environmentMap.Valid())
    { pdf = 0.0f; return { glm::vec3(0.0f), false }; }

  {
    auto bsdf =
      plugin.material.BsdfSample(plugin.material, plugin.random, surface);
    wo = bsdf.wo;
    pdf = bsdf.pdf;
  }
  auto testSurface =
    mt::core::Raycast(scene, surface.origin, wo, surface.triangle);
  if (testSurface.Valid()) { return { glm::vec3(0.0f), false }; }
  auto color = ::SampleEmission(scene, scene.emissionSource.environmentMap, wo);
  return { color * emissionPower, true };
}

mt::PixelInfo SampleWo(
  mt::core::Scene const & scene
, mt::PluginInfo const & /*plugin*/
, mt::core::SurfaceInfo const & /*surface*/
, glm::vec3 const & wo
, float & pdf
) {
  if (!scene.emissionSource.environmentMap.Valid())
    { pdf = 0.0f; return { glm::vec3(0.0f), false }; }

  // TODO
  pdf = 1.0f;

  return {
    ::SampleEmission(scene, scene.emissionSource.environmentMap, wo)
  , true
  };
}

void Precompute(
  mt::core::Scene const & /*scene*/
, mt::core::RenderInfo const & /*render*/
, mt::PluginInfo const & /*plugin*/
) {
}

void UiUpdate(
  mt::core::Scene & scene
, mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  auto const idx = scene.emissionSource.skyboxEmitterPluginIdx;
  if (idx == -1lu || plugin.emitters[idx].PluginLabel() != PluginLabel())
    { return; }

  ImGui::Begin("emitters");
    ImGui::Separator();
    ImGui::Text("environment emitter");
    if (!scene.emissionSource.environmentMap.Valid()) {
      ImGui::TextColored(
        ImVec4(1, 0.25, 0.25, 1)
      , "No environment texture loaded"
      );
    }
    if (ImGui::InputFloat("power", &emissionPower)) {
      render.ClearImageBuffers();
    }
  ImGui::End();
}

bool IsSkybox() { return true; }

} // -- end extern "C"
