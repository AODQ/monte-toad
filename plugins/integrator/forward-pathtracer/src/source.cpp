#include <cr/cr.h>

#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <monte-toad/texture.hpp>

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

  mt::SurfaceInfo surface;
  surface.triangle = triangle;

  surface.distance = std::get<1>(results).distance();
  surface.barycentricUv = std::get<1>(results).barycentricUv;
  surface.normal =
    BarycentricInterpolation(
      triangle->n0, triangle->n1, triangle->n2
    , surface.barycentricUv
    );
  auto surfaceOrigin = origin + wi*surface.distance;
  auto color =
    pluginInfo.material.BsdfFs(
      scene, surface, wi, glm::reflect(wi, surface.normal)
    );

  auto [emissionTri, emissionBarycentricUv] = EmissionSourceTriangle(scene, 0u);
  if (!emissionTri) { return glm::vec3(1.0f, 0.0f, 0.0f); }
  auto emissionOrigin =
    (emissionTri->v0 + emissionTri->v1 + emissionTri->v2) * 0.3333333f;
    /* BarycentricInterpolation( */
    /*   emissionTri->v0, emissionTri->v1, emissionTri->v2, emissionBarycentricUv */
    /* ); */

  auto emissionWo = glm::normalize(emissionOrigin - surfaceOrigin);

  if (glm::dot(emissionWo, surface.normal) <= 0.0f) { return glm::vec3(0.0f); }

  auto [tempTri, tempInt] =
    mt::Raycast(scene, surfaceOrigin, emissionWo, true, triangle);

  if (tempTri && tempTri->meshIdx == emissionTri->meshIdx) { return color; }

  return color * 0.1f;
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
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  spdlog::info("forward pt integrator plugin successfully loaded");

  return 0;
}
