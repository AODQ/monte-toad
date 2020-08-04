// forward path tracer

#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/geometry.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/math.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/spectrum.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <monte-toad/core/triangle.hpp>
#include <monte-toad/debugutil/integratorpathunit.hpp>

#include <mt-plugin/plugin.hpp>

namespace mt::core { struct CameraInfo; }

namespace {

enum class PropagationStatus {
  Continue = 0 // normal behaviour, continue propagation
, IndirectAccumulation = 1 // emitter has been indirectly hit (such as for MIS)
, DirectAccumulation = 2 // emitter has been directly hit, end propagation loop
, End = 3 // no propagation could occur (apply only indirect accumulation)
, IndirectAccumulationEnd = 4 //
};

// joins results such that only the most relevant component is returned
void Join(PropagationStatus & l, PropagationStatus r) {
  // hardcoded case where indirect is asked but path has ended, it should
  // be accepted but the path should still end
  if (
      l == PropagationStatus::End
   && r == PropagationStatus::IndirectAccumulation
  ) {
    l = PropagationStatus::IndirectAccumulationEnd;
    return;
  }

  l = Idx(l) > Idx(r) ? l : r;
}

// TODO move
[[maybe_unused]] float EmitterPdf(
  mt::core::SurfaceInfo const & surface
, mt::core::SurfaceInfo const & emissionSurface
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
  mt::core::Scene const & /*scene*/
, mt::PluginInfo const & /*plugin*/
, mt::core::SurfaceInfo const & /*surface*/
, glm::vec3 const & /*radiance*/
, glm::vec3 & /*accumulatedIrradiance*/
, size_t /*it*/
, void (* /*debugPathRecorder*/)(mt::debugutil::IntegratorPathUnit)
) {
  return PropagationStatus::Continue;
  /* auto propagationStatus = PropagationStatus::Continue; */

  /* /1* if (scene.emissionSource.skyboxEmitterPluginIdx != -1lu) *1/ */
  /* /1* { // apply indirect emission from skybox if available *1/ */
  /* /1*   glm::vec3 emissionWo; *1/ */
  /* /1*   float emissionPdf; *1/ */
  /* /1*   auto info = *1/ */
  /* /1*     plugin *1/ */
  /* /1*       .emitters[scene.emissionSource.skyboxEmitterPluginIdx] *1/ */
  /* /1*       .SampleLi(scene, plugin, surface, emissionWo, emissionPdf); *1/ */

  /* /1*   if (info.valid) { *1/ */
  /* /1*     float bsdfPdf = *1/ */
  /* /1*       plugin.material.BsdfPdf(plugin.material, surface, emissionWo); *1/ */

  /* /1*     // only valid if the bsdfPdf is not delta dirac *1/ */
  /* /1*     if (bsdfPdf > 0.0f) { *1/ */
  /* /1*       glm::vec3 irradiance = *1/ */
  /* /1*         info.color *1/ */
  /* /1*       * radiance *1/ */
  /* /1*       * plugin.material.BsdfFs(plugin.material, surface, emissionWo) *1/ */
  /* /1*       / (emissionPdf/(emissionPdf + bsdfPdf)); *1/ */

  /* /1*       if (glm::length(irradiance) > 0.0001f) { *1/ */
  /* /1*         propagationStatus = PropagationStatus::IndirectAccumulation; *1/ */
  /* /1*         accumulatedIrradiance += irradiance; *1/ */
  /* /1*       } *1/ */
  /* /1*     } *1/ */
  /* /1*   } *1/ */
  /* /1* } *1/ */

  /* { // apply indirect emission from random emitter in scene */
  /*   auto [emissionTriangle, emissionBarycentricUvCoord] = */
  /*     mt::core::EmissionSourceTriangle(scene, plugin); */

  /*   if (!emissionTriangle) { return propagationStatus; } */

  /*   auto emissionOrigin = */
  /*     BarycentricInterpolation( */
  /*       emissionTriangle->v0, emissionTriangle->v1, emissionTriangle->v2 */
  /*     , emissionBarycentricUvCoord */
  /*     ); */

  /*   glm::vec3 emissionWo = glm::normalize(emissionOrigin - surface.origin); */

  /*   float bsdfEmitPdf = */
  /*     mt::core::MaterialIndirectPdf(surface, scene, plugin, emissionWo); */

  /*   // if the surface is delta-dirac, than there is no indirect emission */
  /*   // contribution */
  /*   if (bsdfEmitPdf == 0.0f) { return propagationStatus; } */

  /*   // check that emission source is within reflected hemisphere */
  /*   if (glm::dot(emissionWo, surface.normal) <= 0.0f) */
  /*     { return propagationStatus; } */

  /*   mt::core::SurfaceInfo emissionSurface = */
  /*     mt::core::Raycast(scene, surface.origin, emissionWo, surface.triangle); */

  /*   if (emissionSurface.triangle != emissionTriangle) */
  /*     { return propagationStatus; } */

  /*   float emitPdf = EmitterPdf(surface, emissionSurface); */

  /*   propagationStatus = PropagationStatus::IndirectAccumulation; */
  /*   accumulatedIrradiance += */
  /*     mt::core::MaterialFs( */
  /*       emissionSurface, scene, plugin, glm::vec3(0), true, 0 */
  /*     ) */
  /*   * radiance */
  /*   * mt::core::MaterialFs( */
  /*       surface, scene, plugin, emissionWo, true, 0 */
  /*   / (emitPdf/(emitPdf + bsdfEmitPdf)) */
  /*   ; */


  /*   if (debugPathRecorder) { */
  /*     debugPathRecorder({ */
  /*       radiance, accumulatedIrradiance */
  /*     , mt::TransportMode::Importance, it+2, surface */
  /*     }); */
  /*   } */
  /* } */

  /* return propagationStatus; */
}

PropagationStatus Propagate(
  mt::core::Scene const & scene
, mt::core::SurfaceInfo & surface
, glm::vec3 & radiance
, glm::vec3 & accumulatedIrradiance
, size_t const it
, mt::PluginInfo const & plugin
, void (*debugPathRecorder)(mt::debugutil::IntegratorPathUnit)
) {
  if (debugPathRecorder) {
    debugPathRecorder({
      radiance, accumulatedIrradiance, mt::TransportMode::Radiance, it, surface
    });
  }

  // store a value for current propagation status, which could be overwritten
  auto propagationStatus = PropagationStatus::Continue;

  if (surface.triangle == nullptr) {
    spdlog::critical("could not propagate in a correct manner, null triangle");
    return PropagationStatus::End;
  }

  // generate bsdf sample (this will also be used for next propagation)
  mt::core::BsdfSampleInfo bsdf =
    plugin.material.Sample(surface, scene, plugin);

  // delta-dirac correct pdfs, valid only for direct emissions
  bsdf.pdf = bsdf.pdf == 0.0f ? 1.0f : bsdf.pdf;

  // grab information of next surface
  mt::core::SurfaceInfo nextSurface =
    mt::core::Raycast(scene, plugin, surface.origin, bsdf.wo, surface.triangle);

  // check if an emitter or skybox (which could be a blackbody) was hit
  if (nextSurface.triangle == nullptr) {
    if (scene.emissionSource.skyboxEmitterPluginIdx == -1lu) {
      Join(propagationStatus, PropagationStatus::End);
    } else {
      float pdf;
      auto & emitter =
        plugin.emitters[scene.emissionSource.skyboxEmitterPluginIdx];
      auto color = emitter.SampleWo(scene, plugin, surface, bsdf.wo, pdf);

      if (color.valid) {
        // TODO use pdf
        accumulatedIrradiance += color.color * radiance * bsdf.fs / bsdf.pdf;
        Join(propagationStatus, PropagationStatus::DirectAccumulation);
      } else {
        Join(propagationStatus, PropagationStatus::End);
      }
    }

    // even tho we didn't hit a surface still record the origin
    nextSurface.origin = surface.origin + bsdf.wo*100.0f;
  } else if (plugin.material.IsEmitter(nextSurface, scene, plugin)) {
    auto emissiveColor =
      plugin.material.EmitterFs(nextSurface, scene, plugin);

    /* float emitPdf = EmitterPdf(surface, nextSurface); */

    accumulatedIrradiance +=
      emissiveColor * radiance * bsdf.fs / bsdf.pdf;
      // TODO use below
    /* emissiveColor * radiance * bsdfFs / (bsdfPdf/(emitPdf + bsdfPdf)); */

    propagationStatus = PropagationStatus::DirectAccumulation;
  }

  Join(
    propagationStatus,
    ApplyIndirectEmission(
      scene, plugin, surface, radiance, accumulatedIrradiance
    , it+1, debugPathRecorder
    )
  );

  // contribute to radiance only after emission values are calculated, as it is
  // invalid for indirect emission, and the direct emission might need to handle
  // the PDF in a specific manner anyways
  radiance *= bsdf.fs / bsdf.pdf;

  // -- save raycastinfo
  surface.previousSurface.reset();
  nextSurface.previousSurface =
    std::make_shared<mt::core::SurfaceInfo>(surface);
  surface = nextSurface;

  return propagationStatus;
}

} // -- end anon namespace

extern "C" {

char const * PluginLabel() { return "forward integrator"; }
mt::PluginType PluginType() { return mt::PluginType::Integrator; }

mt::PixelInfo Dispatch(
  glm::vec2 const & uv
, mt::core::Scene const & scene
, mt::core::CameraInfo const & camera
, mt::PluginInfo const & plugin
, mt::core::IntegratorData const & integratorData
, void (*debugPathRecorder)(mt::debugutil::IntegratorPathUnit)
) {
  mt::core::SurfaceInfo surface;
  { // -- apply initial raycast
    auto const eye =
      plugin.camera.Dispatch(
        plugin.random, camera, integratorData.imageResolution, uv
      );

    // store camera info
    if (debugPathRecorder) {
      mt::core::SurfaceInfo cameraSurface;
      cameraSurface.distance = 0.0f;
      cameraSurface.exitting = false;
      cameraSurface.incomingAngle = glm::vec3(0);
      cameraSurface.normal = glm::vec3(0);
      cameraSurface.origin = eye.origin;
      debugPathRecorder({
        glm::vec3(1), glm::vec3(0)
      , mt::TransportMode::Radiance, 0, cameraSurface
      });
    }

    surface =
      mt::core::Raycast(scene, plugin, eye.origin, eye.direction, nullptr);
  }

  // return skybox
  if (!surface.Valid()) {
    if (scene.emissionSource.skyboxEmitterPluginIdx != -1lu) {
      auto & emitter =
        plugin.emitters[scene.emissionSource.skyboxEmitterPluginIdx];
      float pdf;
      return {
        emitter
          .SampleWo(scene, plugin, surface, surface.incomingAngle, pdf)
          .color
      , true
      };
    }
    return mt::PixelInfo{glm::vec3(1.0f, 0.0f, 1.0f), true};
  }

  // check if emitter
  if (plugin.material.IsEmitter(surface, scene, plugin)) {
    auto const emission = plugin.material.EmitterFs(surface, scene, plugin);
    return mt::PixelInfo{emission, true};
  }

  bool hit = false;
  glm::vec3 radiance = glm::vec3(1.0f), accumulatedIrradiance = glm::vec3(0.0f);

  size_t it = 0;
  for (; it < integratorData.pathsPerSample; ++ it) {
    PropagationStatus status =
      Propagate(
        scene
      , surface
      , radiance
      , accumulatedIrradiance
      , it
      , plugin
      , debugPathRecorder
      );

    if (status == PropagationStatus::IndirectAccumulationEnd) {
      hit = true;
      break;
    }
    if (status == PropagationStatus::End) { break; }
    if (status == PropagationStatus::IndirectAccumulation) {
      hit = true;
    }
    if (status == PropagationStatus::DirectAccumulation) {
      hit = true;
      break;
    }

    // apply russian roulette
    float p = glm::max(radiance.r, glm::max(radiance.g, radiance.b));
    if (plugin.random.SampleUniform1() > p) { break; }
    radiance /= p; // add energy lost by other terminated paths
  }

  // store final surface info
  if (hit && debugPathRecorder) {
    debugPathRecorder({
      radiance, accumulatedIrradiance
    , mt::TransportMode::Radiance, it+1, surface
    });
  }

  return mt::PixelInfo { accumulatedIrradiance, hit };
}

bool RealTime() { return false; }

} // -- extern C
