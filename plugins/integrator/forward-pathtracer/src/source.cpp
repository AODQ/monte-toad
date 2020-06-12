#include <cr/cr.h>

#include <monte-toad/enum.hpp>
#include <monte-toad/geometry.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <monte-toad/texture.hpp>

#include <mt-plugin/plugin.hpp>

namespace {

enum class PropagationStatus {
  Continue // normal behaviour, continue propagation
, End // no propagation could occur (apply only indirect accumulation)
, IndirectAccumulation // emitter has been indirectly hit (such as for MIS)
, DirectAccumulation // emmitter has been directly hit, end propagation loop
};

// joins results such that only the most relevant component is returned
[[maybe_unused]] void Join(PropagationStatus & l, PropagationStatus r) {
  l = Idx(l) > Idx(r) ? l : r;
}

// TODO move
[[maybe_unused]] float EmitterPdf(
  mt::SurfaceInfo const & surface
, mt::SurfaceInfo const & emissionSurface
) {
  glm::vec3 const wo = emissionSurface.incomingAngle;
  float inverseDistanceSqr = 1.0f/glm::sqr(glm::length(wo));

  float pdf;
  pdf = glm::InvPi * glm::dot(surface.normal, wo);
  pdf *= inverseDistanceSqr;
  /* float const surfaceArea = emissionTri->area(); */
  return pdf;
}

PropagationStatus ApplyIndirectEmission(
  mt::Scene const & scene
, mt::PluginInfo const & plugin
, mt::SurfaceInfo const & surface
, glm::vec3 const & radiance
, glm::vec3 & accumulatedIrradiance
) {
  auto propagationStatus = PropagationStatus::Continue;

  if (scene.emissionSource.skyboxEmitterPluginIdx != -1)
  { // apply indirect emission from skybox if available
    glm::vec3 emissionWo;
    float emissionPdf;
    auto info =
      plugin
        .emitters[scene.emissionSource.skyboxEmitterPluginIdx]
        .SampleLi(scene, plugin, surface, emissionWo, emissionPdf);

    if (info.valid) {
      float bsdfPdf = plugin.material.BsdfPdf(surface, emissionWo);

      // only valid if the bsdfPdf is not delta dirac
      if (bsdfPdf > 0.0f) {
        glm::vec3 irradiance =
          info.color
        * radiance
        * plugin.material.BsdfFs(scene, surface, emissionWo)
        / (emissionPdf/(emissionPdf + bsdfPdf));

        if (glm::length(irradiance) > 0.0001f) {
          propagationStatus = PropagationStatus::IndirectAccumulation;
          accumulatedIrradiance += irradiance;
        }
      }
    }
  }

  { // apply indirect emission from random emitter in scene
    auto [emissionTriangle, emissionBarycentricUvCoord] =
      mt::EmissionSourceTriangle(scene, plugin.random);

    if (!emissionTriangle) { return propagationStatus; }

    auto emissionOrigin =
      BarycentricInterpolation(
        emissionTriangle->v0, emissionTriangle->v1, emissionTriangle->v2
      , emissionBarycentricUvCoord
      );

    glm::vec3 emissionWo = glm::normalize(emissionOrigin - surface.origin);

    float bsdfEmitPdf = plugin.material.BsdfPdf(surface, emissionWo);

    // if the surface is delta-dirac, than there is no indirect emission
    // contribution
    if (bsdfEmitPdf == 0.0f) { return propagationStatus; }

    // check that emission source is within reflected hemisphere
    if (glm::dot(emissionWo, surface.normal) <= 0.0f)
      { return propagationStatus; }

    mt::SurfaceInfo emissionSurface =
      mt::Raycast(scene, surface.origin, emissionWo, surface.triangle);

    if (emissionSurface.triangle != emissionTriangle)
      { return propagationStatus; }

    float emitPdf = EmitterPdf(surface, emissionSurface);

    propagationStatus = PropagationStatus::IndirectAccumulation;
    accumulatedIrradiance +=
      plugin.material.BsdfFs(scene, emissionSurface, glm::vec3(0))
    * radiance
    * plugin.material.BsdfFs(scene, surface, emissionWo)
    / (emitPdf/(emitPdf + bsdfEmitPdf))
    ;
  }

  return propagationStatus;
}

PropagationStatus Propagate(
  mt::Scene const & scene
, mt::SurfaceInfo & surface
, glm::vec3 & radiance
, glm::vec3 & accumulatedIrradiance
, size_t const it
, mt::RenderInfo const & render
, mt::PluginInfo const & plugin
) {

  // store a vlaue for current propagation status, which could be overwritten
  auto propagationStatus = PropagationStatus::Continue;

  // generate bsdf sample (this will also be used for next propagation)
  auto [bsdfWo, bsdfPdf] =
    plugin.material.BsdfSample(plugin.random, surface);
  auto bsdfFs = plugin.material.BsdfFs(scene, surface, bsdfWo);

  // delta-dirac correct pdfs, valid only for direct emissions
  bsdfPdf = bsdfPdf == 0.0f ? 1.0f : bsdfPdf;

  // grab information of next surface
  mt::SurfaceInfo nextSurface =
    mt::Raycast(scene, surface.origin, bsdfWo, surface.triangle);

  // check if an emitter or skybox (which could be a blackbody) was hit
  if (nextSurface.triangle == nullptr) {
    Join(
      propagationStatus,
      PropagationStatus::End
    );
  } else if (plugin.material.IsEmitter(scene, *nextSurface.triangle)) {
    auto emissiveColor = plugin.material.BsdfFs(scene, nextSurface, bsdfWo);

    /* float emitPdf = EmitterPdf(surface, nextSurface); */

    accumulatedIrradiance +=
      emissiveColor * radiance * bsdfFs / bsdfPdf;
      // TODO use below
      /* emissiveColor * radiance * bsdfFs / (bsdfPdf/(emitPdf + bsdfPdf)); */

    propagationStatus = PropagationStatus::DirectAccumulation;
  }

  Join(
    propagationStatus,
    ApplyIndirectEmission(
      scene, plugin, surface, radiance, accumulatedIrradiance
    )
  );

  // contribute to radiance only after emission values are calculated, as it is
  // invalid for indirect emission, and the direct emission might need to handle
  // the PDF in a specific manner anyways
  radiance *= bsdfFs / bsdfPdf;

  // -- save raycastinfo if intersection was valid
  if (nextSurface.triangle)
    { surface = nextSurface; }

  return propagationStatus;
}

mt::PixelInfo Dispatch(
  glm::vec2 const & uv
, mt::Scene const & scene
, mt::RenderInfo const & renderInfo
, mt::PluginInfo const & plugin
, mt::IntegratorData const & integratorData
) {
  mt::SurfaceInfo surface;
  { // -- apply initial raycast
    auto [origin, wi] =
      plugin.camera.Dispatch(
        plugin.random, renderInfo, integratorData.imageResolution, uv
      );
    surface = mt::Raycast(scene, origin, wi, nullptr);
  }

  // return skybox
  if (!surface.Valid()) {
    return mt::PixelInfo{glm::vec3(1.0f, 0.0f, 1.0f), true};
  }

  // check if emitter
  if (plugin.material.IsEmitter(scene, *surface.triangle)) {
    auto const emission = plugin.material.BsdfFs(scene, surface, glm::vec3(0));
    return mt::PixelInfo{emission, true};
  }

  bool hit = false;
  glm::vec3 radiance = glm::vec3(1.0f), accumulatedIrradiance = glm::vec3(0.0f);

  for (size_t it = 0; it < integratorData.pathsPerSample; ++ it) {
    PropagationStatus status =
      Propagate(
        scene
      , surface
      , radiance
      , accumulatedIrradiance
      , it
      , renderInfo
      , plugin
      );

    if (status == PropagationStatus::End) { break; }
    if (status == PropagationStatus::IndirectAccumulation) {
      hit = true;
    }
    if (status == PropagationStatus::DirectAccumulation) {
      hit = true;
      break;
    }
  }

  return mt::PixelInfo { accumulatedIrradiance, hit };
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
      integrator.realtime = false;
      integrator.useGpu = false;
      integrator.Dispatch = &Dispatch;
      integrator.pluginType = mt::PluginType::Integrator;
      integrator.pluginLabel = "forward pathtracer";
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  return 0;
}
