#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/texture.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace {

static glm::vec3 emissionColor = glm::vec3(1.0f);
static float emissionPower = 1.0f;

} // -- end anon namespace

extern "C" {

char const * PluginLabel() { return "furnace emitter"; }
mt::PluginType PluginType() { return mt::PluginType::Emitter; }

mt::PixelInfo SampleLi(
  mt::Scene const & scene
, mt::PluginInfo const & plugin
, mt::SurfaceInfo const & surface
, glm::vec3 & wo
, float & pdf
) {
  {
    auto [wo0, pdf0] =
      plugin.material.BsdfSample(plugin.material, plugin.random, surface);
    wo = wo0;
    pdf = pdf0;
  }
  auto testSurface = mt::Raycast(scene, surface.origin, wo, surface.triangle);
  if (testSurface.Valid()) { return { glm::vec3(0.0f), false }; }
  auto color = emissionColor * emissionPower;
  return { color * emissionPower, true };
}

mt::PixelInfo SampleWo(
  mt::Scene const & scene
, mt::PluginInfo const & plugin
, mt::SurfaceInfo const & surface
, glm::vec3 const & wo
, float & pdf
) {
  // TODO
  pdf = 1.0f;

  return {
    emissionColor * emissionPower,
    true
  };
}

void Precompute(
  mt::Scene const & scene
, mt::RenderInfo const & render
, mt::PluginInfo const & plugin
) {
}

void UiUpdate(
  mt::Scene & scene
, mt::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  auto idx = scene.emissionSource.skyboxEmitterPluginIdx;
  if (idx == -1 || plugin.emitters[idx].PluginLabel() != ::PluginLabel())
    { return; }

  ImGui::Begin("emitters");
    ImGui::Separator();
    ImGui::Text("furnace emitter");
    if (
        ImGui::InputFloat("power", &emissionPower)
     || ImGui::ColorEdit3("color", &emissionColor.r)
    ) {
      render.ClearImageBuffers();
    }
  ImGui::End();
}

bool IsSkybox() { return true; }

} // -- end extern "C"