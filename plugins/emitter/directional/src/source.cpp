// directional emitter

#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace {

static glm::vec3 emissionDirection = glm::vec3(0.0f, 0.0f, 1.0f);
static glm::vec3 emissionColor = glm::vec3(1.0f);
static float emissionPower = 1.0f;

} // -- namespace

extern "C" {

char const * PluginLabel() { return "directional emitter"; }
mt::PluginType PluginType() { return mt::PluginType::Emitter; }

mt::PixelInfo SampleLi(
  mt::core::Scene const & scene
, mt::PluginInfo const & plugin
, mt::core::SurfaceInfo const & surface
, glm::vec3 & wo
, float & pdf
) {
  wo = emissionDirection;
  pdf = 1.0f;
  auto testSurface =
    mt::core::Raycast(scene, plugin, surface.origin, wo, surface.triangle);
  if (testSurface.Valid()) { return { glm::vec3(0.0f), false }; }
  // TODO TOAD apparently multiply by area
  return { emissionColor * emissionPower, true };
}

mt::PixelInfo SampleWo(
  mt::core::Scene const & /*scene*/
, mt::PluginInfo const & /*plugin*/
, mt::core::SurfaceInfo const & /*surface*/
, glm::vec3 const & /*wo*/
, float & pdf
) {
  pdf = 0.0f; return { glm::vec3(0.0f), false };
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
  if (idx == -1lu || plugin.emitters[idx].PluginLabel() != ::PluginLabel())
    { return; }

  ImGui::Begin("emitters");
    ImGui::Separator();
    ImGui::Text("directional emitter");
    if (ImGui::InputFloat3("direction", &emissionDirection.x)) {
      emissionDirection = glm::normalize(emissionDirection);
      render.ClearImageBuffers();
    }
    if (
        ImGui::ColorPicker3("color", &emissionColor.r)
     || ImGui::InputFloat("power", &emissionPower)
    ) {
      render.ClearImageBuffers();
    }
  ImGui::End();
}

bool IsSkybox() { return true; }

} // -- end extern C
