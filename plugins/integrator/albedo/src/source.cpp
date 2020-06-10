#include <cr/cr.h>

#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <monte-toad/texture.hpp>
#include <mt-plugin/plugin.hpp>

#include <cr/cr.h>
#include <imgui/imgui.hpp>

namespace {

bool CR_STATE applyFogging = true;

mt::PixelInfo Dispatch(
  glm::vec2 const & uv
, mt::Scene const & scene
, mt::RenderInfo const & renderInfo
, mt::PluginInfo const & pluginInfo
, mt::IntegratorData const & integratorData
) {
  auto [origin, wi] =
    pluginInfo.camera.Dispatch(
      pluginInfo.random, renderInfo, integratorData.imageResolution, uv
    );

  auto surface = mt::Raycast(scene, origin, wi, nullptr);
  if (!surface.Valid()) { return mt::PixelInfo{glm::vec3(0.0f), false}; }

  auto color =
    pluginInfo.material.BsdfFs(
      scene, surface, glm::reflect(wi, surface.normal)
    );

  // do fogging just for some visual characteristics if requested
  if (applyFogging) {
    float distance = surface.distance;
    distance /= glm::length(scene.bboxMax - scene.bboxMin);
    color *= glm::exp(-distance * 2.0f);
  }

  return mt::PixelInfo{color, true};
}

void UiUpdate(
  mt::Scene & scene
, mt::RenderInfo & render
, mt::PluginInfo const & plugin
, mt::IntegratorData & integratorData
) {
  ImGui::Begin("albedo raycaster (config)");
  if (ImGui::Checkbox("apply fog", &applyFogging))
    { integratorData.Clear(); }
  ImGui::End();
}

}

CR_EXPORT int cr_main(struct cr_plugin * ctx, enum cr_op operation) {
  // return immediately if an update, this won't do anything
  if (operation == CR_STEP || operation == CR_UNLOAD) { return 0; }
  if (!ctx || !ctx->userdata) { return 0; }

  auto & integrator =
    *reinterpret_cast<mt::PluginInfoIntegrator*>(ctx->userdata);

  switch (operation) {
    case CR_LOAD:
      integrator.realtime = true;
      integrator.useGpu = false;
      integrator.Dispatch = &Dispatch;
      integrator.UiUpdate = &UiUpdate;
      integrator.pluginType = mt::PluginType::Integrator;
      integrator.pluginLabel = "albedo raycaster";
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  spdlog::info("albedo integrator plugin successfully loaded");

  return 0;
}
