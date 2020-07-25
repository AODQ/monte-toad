// depth integrator

#include <monte-toad/integratordata.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>

#include <mt-plugin/plugin.hpp>

namespace mt { struct CameraInfo; }

extern "C" {

char const * PluginLabel() { return "depth integrator"; }
mt::PluginType PluginType() { return mt::PluginType::Integrator; }

mt::PixelInfo Dispatch(
  glm::vec2 const & uv
, mt::Scene const & scene
, mt::CameraInfo const & camera
, mt::PluginInfo const & pluginInfo
, mt::IntegratorData const & integratorData
, void (*)(mt::debugutil::IntegratorPathUnit)
) {
  auto const eye =
    pluginInfo.camera.Dispatch(
      pluginInfo.random, camera, integratorData.imageResolution, uv
    );

  auto surface = mt::Raycast(scene, eye.origin, eye.direction, nullptr);
  if (!surface.Valid()) { return mt::PixelInfo{glm::vec3(0.0f), false}; }

  float distance = surface.distance;
  distance /= glm::length(scene.bboxMax - scene.bboxMin);

  return mt::PixelInfo{glm::vec3(1.0f - glm::exp(-distance)), true};
}

bool RealTime() { return true; }

} // -- end extern "C"
