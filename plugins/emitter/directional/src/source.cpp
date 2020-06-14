#include <cr/cr.h>

#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace {

static CR_STATE glm::vec3 emissionDirection = glm::vec3(0.0f, 0.0f, 1.0f);
static CR_STATE glm::vec3 emissionColor = glm::vec3(1.0f);
static CR_STATE float emissionPower = 1.0f;

static char const * PluginLabel = "directional emitter";

mt::PixelInfo SampleLi(
  mt::Scene const & scene
, mt::PluginInfo const & plugin
, mt::SurfaceInfo const & surface
, glm::vec3 & wo
, float & pdf
) {
  wo = emissionDirection;
  pdf = 1.0f;
  auto testSurface = mt::Raycast(scene, surface.origin, wo, surface.triangle);
  if (testSurface.Valid()) { return { glm::vec3(0.0f), false }; }
  // TODO TOAD apparently multiply by area
  return { emissionColor * emissionPower, true };
}

mt::PixelInfo SampleWo(
  mt::Scene const & scene
, mt::PluginInfo const & plugin
, mt::SurfaceInfo const & surface
, glm::vec3 const & wo
, float & pdf
) {
  pdf = 0.0f; return { glm::vec3(0.0f), false };
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
  auto pluginIdx = scene.emissionSource.skyboxEmitterPluginIdx;
  if (pluginIdx == -1 || plugin.emitters[pluginIdx].pluginLabel != PluginLabel)
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

}

CR_EXPORT int cr_main(struct cr_plugin * ctx, enum cr_op operation) {
  // return immediately if an update, this won't do anything
  if (operation == CR_STEP || operation == CR_UNLOAD) { return 0; }
  if (!ctx || !ctx->userdata) { return 0; }

  auto & emitter =
    *reinterpret_cast<mt::PluginInfoEmitter*>(ctx->userdata);

  switch (operation) {
    case CR_LOAD:
      emitter.isSkybox = true; // TODO TOAD technically this isn't a skybox
      emitter.SampleLi = &SampleLi;
      emitter.SampleWo = &SampleWo;
      emitter.UiUpdate = &UiUpdate;
      emitter.Precompute = &Precompute;
      emitter.pluginType = mt::PluginType::Emitter;
      emitter.pluginLabel = PluginLabel;
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  return 0;
}
