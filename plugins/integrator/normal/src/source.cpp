#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/texture.hpp>

#include <mt-plugin/plugin.hpp>

extern "C" {

char const * PluginLabel() { return "normal integrator"; }
mt::PluginType PluginType() { return mt::PluginType::Integrator; }

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

  return mt::PixelInfo{surface.normal, true};
}

bool RealTime() { return true; }

} // -- end extern "C"
