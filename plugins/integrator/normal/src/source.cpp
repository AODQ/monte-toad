// normal integrator

#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/surfaceinfo.hpp>

#include <imgui/imgui.hpp>

#include <mt-plugin/plugin.hpp>

namespace mt::core { struct CameraInfo; }

namespace {

bool normalizedSpace;

} // -- anon namespace

extern "C" {

char const * PluginLabel() { return "normal integrator"; }
mt::PluginType PluginType() { return mt::PluginType::Integrator; }

mt::PixelInfo DispatchRealtime(
  glm::vec2 const & /*uv*/
, mt::core::SurfaceInfo const & surface
, mt::core::Scene const & /*scene*/
, mt::PluginInfo const & /*plugin*/
, mt::core::IntegratorData const & /*integratorData*/
) {
  if (!surface.Valid()) { return mt::PixelInfo{glm::vec3(0.0f), false}; }

  auto surfaceNormal = surface.normal;

  // normalize from range -1 ‥ 1 to 0 ‥ 1
  if (::normalizedSpace)
    { surfaceNormal = surface.normal*0.5f + glm::vec3(0.5f); }

  return mt::PixelInfo{surfaceNormal, true};
}

bool RealTime() { return true; }

void UiUpdate(
  mt::core::Scene & /*scene*/
, mt::core::RenderInfo & /*render*/
, mt::PluginInfo const & /*plugin*/
, mt::core::IntegratorData & integratorData
) {
  ImGui::Begin("normal integrator (config)");
    if (ImGui::Checkbox("normalized space", &::normalizedSpace))
      { mt::core::Clear(integratorData); }
  ImGui::End();
}

} // -- extern "C"
