// depth integrator

#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/surfaceinfo.hpp>

#include <mt-plugin/plugin.hpp>

namespace mt::core { struct CameraInfo; }

extern "C" {

char const * PluginLabel() { return "depth integrator"; }
mt::PluginType PluginType() { return mt::PluginType::Integrator; }

mt::PixelInfo DispatchRealtime(
  glm::vec2 const & /*uv*/
, mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & /*plugin*/
, mt::core::IntegratorData const & /*integratorData*/
) {
  if (!surface.Valid()) { return mt::PixelInfo{glm::vec3(0.0f), false}; }

  float distance = surface.distance;
  distance /= glm::length(scene.bboxMax - scene.bboxMin);

  return mt::PixelInfo{glm::vec3(1.0f - glm::exp(-distance)), true};
}

bool RealTime() { return true; }

} // -- end extern "C"
