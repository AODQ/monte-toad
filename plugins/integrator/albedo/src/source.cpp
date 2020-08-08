// albedo integrator

#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace mt::core { struct CameraInfo; }
namespace mt::core { struct RenderInfo; }

extern "C" {

char const * PluginLabel() { return "albedo integrator"; }
mt::PluginType PluginType() { return mt::PluginType::Integrator; }

mt::PixelInfo Dispatch(
  glm::vec2 const & uv
, mt::core::Scene const & scene
, mt::core::CameraInfo const & camera
, mt::PluginInfo const & plugin
, mt::core::IntegratorData const & integratorData
, void (*)(mt::debugutil::IntegratorPathUnit)
) {
  auto const eye =
    plugin.camera.Dispatch(
      plugin.random, camera, integratorData.imageResolution, uv
    );

  auto const surface =
    mt::core::Raycast(scene, plugin, eye.origin, eye.direction, -1ul);

  if (!surface.Valid()) { return mt::PixelInfo{glm::vec3(0.0f), false}; }

  auto color = plugin.material.AlbedoApproximation(surface, scene, plugin);

  // do fogging just for some visual characteristics if requested
  /* if (applyFogging) { */
  /*   float distance = surface.distance; */
  /*   distance /= glm::length(scene.bboxMax - scene.bboxMin); */
  /*   color *= glm::exp(-distance * 2.0f); */
  /* } */

  return mt::PixelInfo{color, true};
}

void UiUpdate(
  mt::core::Scene & /*scene*/
, mt::core::RenderInfo & /*render*/
, mt::PluginInfo const & /*plugin*/
, mt::core::IntegratorData & /*integratorData*/
) {
  ImGui::Begin("albedo integrator (config)");
    /* if (ImGui::Checkbox("apply fog", &applyFogging)) */
    /*   { mt::Clear(integratorData); } */
  ImGui::End();
}

bool RealTime() { return true; }

} // end extern "C"
