#include <cr/cr.h>

#include <monte-toad/scene.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>

#include <mt-plugin/plugin.hpp>

namespace {
glm::vec3 Dispatch(
  glm::vec2 const & uv, glm::vec2 const & resolution
, mt::Scene const & scene
, mt::RenderInfo const & renderInfo
, mt::PluginInfo const & pluginInfo
) {
  auto [origin, wi] =
    pluginInfo.camera.Dispatch(pluginInfo.random, renderInfo, uv);

  auto results = mt::Raycast(scene, origin, wi, true, nullptr);
  auto const * triangle = std::get<0>(results);
  if (!triangle) { return glm::vec3(0.0f); }

  return
    BarycentricInterpolation(
      triangle->n0, triangle->n1, triangle->n2
    , std::get<1>(results).barycentricUv
    );
}
}

CR_EXPORT int cr_main(struct cr_plugin * ctx, enum cr_op operation) {
  // return immediately if an update, this won't do anything
  if (operation == CR_STEP) { return 0; }

  if (!ctx) {
    spdlog::error("error loading normal integrator plugin, no context\n");
    return -1;
  }
  if (!ctx->userdata) {
    spdlog::error("error loading normal integrator plugin, no userdata\n");
    return -1;
  }

  auto & integrator =
    *reinterpret_cast<mt::PluginInfoIntegrator*>(ctx->userdata);

  switch (operation) {
    case CR_LOAD:
      integrator.realtime = true;
      integrator.useGpu = false;
      integrator.Dispatch = &Dispatch;
      integrator.pluginType = mt::PluginType::Integrator;
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  spdlog::info("normal integrator plugin successfully loaded");

  return 0;
}
