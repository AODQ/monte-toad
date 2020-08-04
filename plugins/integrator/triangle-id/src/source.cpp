// triangle ID integrator

#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/surfaceinfo.hpp>

#include <mt-plugin/plugin.hpp>

namespace mt::core { struct CameraInfo; }

extern "C" {

char const * PluginLabel() { return "triangle ID integrator"; }
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
    mt::core::Raycast(scene, plugin, eye.origin, eye.direction, nullptr);

  if (!surface.Valid()) { return mt::PixelInfo{glm::vec3(0.0f), false}; }

  glm::vec3 color;
  auto t = reinterpret_cast<size_t>(surface.triangle);
  color.r = (t % 255) / 255.0f;
  color.g = (t % 4096) / 4096.0f;
  color.b = (t % 6555) / 6555.0f;

  return mt::PixelInfo{color, true};
}

bool RealTime() { return true; }

} // -- end extern "C"
