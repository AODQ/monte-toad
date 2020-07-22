#include <monte-toad/camerainfo.hpp>
#include <monte-toad/integratordata.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <monte-toad/texture.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

extern "C" {

char const * PluginLabel() { return "albedo integrator"; }
mt::PluginType PluginType() { return mt::PluginType::Integrator; }

mt::PixelInfo Dispatch(
  glm::vec2 const & uv
, mt::Scene const & scene
, mt::CameraInfo const & camera
, mt::PluginInfo const & pluginInfo
, mt::IntegratorData const & integratorData
, void (*debugPathRecorder)(mt::debugutil::IntegratorPathUnit)
) {
  auto [origin, wi] =
    pluginInfo.camera.Dispatch(
      pluginInfo.random, camera, integratorData.imageResolution, uv
    );

  auto surface = mt::Raycast(scene, origin, wi, nullptr);
  if (!surface.Valid()) { return mt::PixelInfo{glm::vec3(0.0f), false}; }

  auto color =
    pluginInfo.material.BsdfFs(
      pluginInfo.material, surface, glm::reflect(wi, surface.normal)
    );

  // do fogging just for some visual characteristics if requested
  /* if (applyFogging) { */
  /*   float distance = surface.distance; */
  /*   distance /= glm::length(scene.bboxMax - scene.bboxMin); */
  /*   color *= glm::exp(-distance * 2.0f); */
  /* } */

  return mt::PixelInfo{color, true};
}

void UiUpdate(
  mt::Scene & scene
, mt::RenderInfo & render
, mt::PluginInfo const & plugin
, mt::IntegratorData & integratorData
) {
  ImGui::Begin("albedo integrator (config)");
    /* if (ImGui::Checkbox("apply fog", &applyFogging)) */
    /*   { mt::Clear(integratorData); } */
  ImGui::End();
}

bool RealTime() { return true; }

} // end extern "C"
