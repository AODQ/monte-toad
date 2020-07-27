// normal integrator

#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/surfaceinfo.hpp>

#include <mt-plugin/plugin.hpp>

namespace mt::core { struct CameraInfo; }

extern "C" {

char const * PluginLabel() { return "normal integrator"; }
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

  auto surface = mt::core::Raycast(scene, eye.origin, eye.direction, nullptr);
  if (!surface.Valid()) { return mt::PixelInfo{glm::vec3(0.0f), false}; }

  return mt::PixelInfo{surface.normal, true};
}

bool RealTime() { return true; }

} // -- end extern "C"
