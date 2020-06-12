#include <cr/cr.h>

#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>

#include <mt-plugin/plugin.hpp>

namespace {
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

  float distance = surface.distance;
  distance /= glm::length(scene.bboxMax - scene.bboxMin);

  return mt::PixelInfo{glm::vec3(1.0f - glm::exp(-distance)), true};
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
      integrator.pluginType = mt::PluginType::Integrator;
      integrator.pluginLabel = "depth raycaster";
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  return 0;
}
