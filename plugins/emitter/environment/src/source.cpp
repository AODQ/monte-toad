// environmental emitter

#include <monte-toad/imgui.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <monte-toad/texture.hpp>
#include <mt-plugin/plugin.hpp>

namespace {

static float emissionPower = 1.0f;

} // -- namespace

extern "C" {

char const * PluginLabel() { return "environment emitter"; }
mt::PluginType PluginType() { return mt::PluginType::Emitter; }

mt::PixelInfo SampleLi(
  mt::Scene const & scene
, mt::PluginInfo const & plugin
, mt::SurfaceInfo const & surface
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
  auto testSurface = mt::Raycast(scene, surface.origin, wo, surface.triangle);
  if (testSurface.Valid()) { return { glm::vec3(0.0f), false }; }
  auto color = mt::Sample(scene.emissionSource.environmentMap, wo);
  return { color * emissionPower, true };
}

mt::PixelInfo SampleWo(
  mt::Scene const & scene
, mt::PluginInfo const & /*plugin*/
, mt::SurfaceInfo const & /*surface*/
, glm::vec3 const & wo
, float & pdf
) {
  if (!scene.emissionSource.environmentMap.Valid())
    { pdf = 0.0f; return { glm::vec3(0.0f), false }; }

  // TODO
  pdf = 1.0f;

  return {
    mt::Sample(scene.emissionSource.environmentMap, wo) * emissionPower,
    true
  };
}

void Precompute(
  mt::Scene const & /*scene*/
, mt::RenderInfo const & /*render*/
, mt::PluginInfo const & /*plugin*/
) {
}

void UiUpdate(
  mt::Scene & scene
, mt::RenderInfo & render
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
