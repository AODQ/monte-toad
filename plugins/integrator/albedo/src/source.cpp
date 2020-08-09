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

mt::PixelInfo DispatchRealtime(
  glm::vec2 const & uv
, mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
, mt::core::IntegratorData const & integratorData
) {
  // TODO return the skybox
  if (!surface.Valid()) { return mt::PixelInfo{glm::vec3(0.0f), false}; }

  auto color = plugin.material.AlbedoApproximation(surface, scene, plugin);
  return mt::PixelInfo{color, true};
}

void UiUpdate(
  mt::core::Scene & /*scene*/
, mt::core::RenderInfo & /*render*/
, mt::PluginInfo const & /*plugin*/
, mt::core::IntegratorData & /*integratorData*/
) {
  ImGui::Begin("albedo integrator (config)");
  ImGui::End();
}

bool RealTime() { return true; }

} // end extern "C"
