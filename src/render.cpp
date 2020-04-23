#include "render.hpp"

#include "log.hpp"
#include "math.hpp"
#include "noise.hpp"
#include "primitive.hpp"
#include "scene.hpp"
#include "texture.hpp"

#include <random>

namespace {

////////////////////////////////////////////////////////////////////////////////
glm::vec3 LookAt(glm::vec3 dir, glm::vec2 uv, glm::vec3 up, float fovRadians) {
  glm::vec3
    ww = glm::normalize(dir),
    uu = glm::normalize(glm::cross(ww, up)),
    vv = glm::normalize(glm::cross(uu, ww));
  return
    glm::normalize(
      glm::mat3(uu, vv, ww)
    * glm::vec3(uv.x, uv.y, fovRadians)
    );
}

////////////////////////////////////////////////////////////////////////////////
struct PixelInfo {
  PixelInfo() = default;

  glm::vec2 uv = glm::vec2(0.0f);
  glm::vec2 dimensions = glm::vec2(0.0f);
  GenericNoiseGenerator * noise = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
struct RaycastInfo {
  RaycastInfo() = default;
  Triangle const * triangle;
  Intersection intersection;
  glm::vec3 normal;
  glm::vec2 uv;

  static RaycastInfo Construct(
    Triangle const * triangle
  , Intersection intersection
  , glm::vec3 const & wi
  ) {
    RaycastInfo self;
    self.triangle = triangle;
    self.intersection = intersection;

    // -- unpack triangle data if valid
    if (self.triangle) {
      self.normal =
        glm::normalize(
          BarycentricInterpolation(
            self.triangle->n0 , self.triangle->n1 , self.triangle->n2
          , self.intersection.barycentricUv
          )
        );

      // might want to flip normal dependent on direction
      if (glm::dot(-wi, self.normal) < 0.0f) { self.normal *= -1.0f; }

      self.uv =
        glm::normalize(
          BarycentricInterpolation(
            self.triangle->uv0 , self.triangle->uv1 , self.triangle->uv2
          , self.intersection.barycentricUv
          )
        );
    }

    return self;
  }
};

///////////////////////////////////////////////////////////////////////////////u/
std::pair<glm::vec3, glm::vec3> CalculateXY(glm::vec3 const & normal) {
  glm::vec3 binormal =
    glm::abs(normal.x) > 0.1f
  ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);

  binormal = glm::normalize(glm::cross(normal, binormal));
  glm::vec3 bitangent = (glm::cross(binormal, normal));
  return std::make_pair(binormal, bitangent);
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 ReorientHemisphere(glm::vec3 wo, glm::vec3 normal) {
  auto [binormal, bitangent] = CalculateXY(normal);
  return bitangent*wo.x + binormal*wo.y + normal*wo.z;
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 ToCartesian(float cosTheta, float phi) {
  float sinTheta = glm::sqrt(glm::max(0.0f, 1.0f - cosTheta));
  return glm::vec3(glm::cos(phi)*sinTheta, glm::sin(phi)*sinTheta, cosTheta);
}

////////////////////////////////////////////////////////////////////////////////
float BsdfPdf(RaycastInfo const & results, glm::vec3 wi, glm::vec3 wo) {
  return glm::InvPi;
  /* auto cosTheta = glm::dot(wo, results.normal); */
  /* return cosTheta < 0.0f ? 0.0f : 1.0f/(cosTheta) * glm::InvPi; */
  /* return cosTheta < 0.0f ? 0.0f : 1.0f/(cosTheta) * glm::InvPi; */
}

////////////////////////////////////////////////////////////////////////////////
std::pair<glm::vec3 /*wo*/, float /*pdf*/> BsdfSample(
  PixelInfo & pixelInfo
, RaycastInfo const & results
, glm::vec3 const & wi
) {
  glm::vec2 u = SampleUniform2(*pixelInfo.noise);

  glm::vec3 wo =
    ReorientHemisphere(
      glm::normalize(ToCartesian(glm::sqrt(u.y), glm::Tau*u.x))
    , results.normal
    );
  return std::make_pair(wo, BsdfPdf(results, wi, wo));

  // -- uniform weighted hemisphere
  /* float const r = glm::sqrt(1.0f - u.x*u.x); */
  /* float const phi = 2.0f * glm::Pi * u.y; */
  /* auto wo = */
  /*   ReorientHemisphere( */
  /*     glm::vec3(glm::cos(phi) * r, glm::sin(phi) * r, u.x), results.normal */
  /*   ); */
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 BsdfFs(
  Scene const & scene
, RaycastInfo const & results
, glm::vec3 wi, glm::vec3 wo
) {
  auto const & mesh = scene.meshes[results.triangle->meshIdx];

  glm::vec3 baseColor = glm::vec3(0.88f);
  if (mesh.material.baseColorTextureIdx != static_cast<size_t>(-1)) {
    baseColor =
      SampleBilinear(
        scene.textures[mesh.material.baseColorTextureIdx]
      , results.uv
      );
  } else {
    baseColor = mesh.material.colorDiffuse;
  }

  return baseColor * glm::dot(results.normal, wo);

  /* glm::vec3 colorSpecular = glm::vec3(0.88f); */
  /* colorSpecular = baseColor; */
  /* if (mesh.material.baseColorTextureIdx == static_cast<size_t>(-1)) { */
  /*   colorSpecular = mesh.material.colorSpecular; */
  /* } */

  /* float roughness = 0.88f; */
  /* if (mesh.material.diffuseRoughnessTextureIdx != static_cast<size_t>(-1)) { */
  /*   // ... TODO ... */
  /* } else { */
  /*   // approximation of shininess to roughness */
  /*   roughness = */
  /*     glm::sqrt(2.0f/(glm::max(mesh.material.shininess, 1.0f) + 1.0f)); */
  /* } */

  /* float cosNWo = glm::dot(results.normal, wo); */
  /* float cosNWo2 = SQR(cosNWo); */

  /* float roughnessDividend = */
  /*   glm::Pi * glm::sqr((roughness - 1.0f) * cosNWo2 + 1.0f); */

  /* // GGX for now */
  /* roughness *= roughness; */
  /* float distribution = roughness/roughnessDividend; */


  /* // temporarily calculate pdf here until it is importance sampled */
  /* float bsdfPdf = (roughness * cosNWo)/roughnessDividend; */

  /* auto color = (baseColor / glm::Pi) + (colorSpecular * distribution); */

  /* return (cosNWo * color) / bsdfPdf; */
  /* /1* return glm::clamp(color / bsdfPdf, glm::vec3(0.0f), glm::vec3(1.0f)); *1/ */
}

////////////////////////////////////////////////////////////////////////////////
float SphericalTheta(glm::vec3 const & v) {
  (void)SphericalTheta; // FIXME TODO
  return glm::acos(glm::clamp(v.z, -1.0f, +1.0f));
}

////////////////////////////////////////////////////////////////////////////////
float SphericalPhi(glm::vec3 const & v) {
  (void)SphericalPhi; // FIXME TODO
  float p = glm::atan(v.y, v.x);
  return (p < 0.0f) ? (p + 2.0f*glm::Pi) : p;
}

////////////////////////////////////////////////////////////////////////////////
enum class PropagationStatus {
  Continue // normal behaviour, continue propagation
, End // no propagation could occur (apply only indirect accumulation)
, IndirectAccumulation // emitter has been indirectly hit (such as for MIS)
, DirectAccumulation // emitter has been directlly hit, end propagation loop
};

PropagationStatus Propagate(
  PixelInfo & pixelInfo
, Scene const & scene
, RaycastInfo & results
, glm::vec3 & radiance
, glm::vec3 & accumulatedIrradiance
, glm::vec3 & ro
, glm::vec3 & wi
, uint16_t step
, bool useBvh
) {
  // -- check if skybox (if available) was hit
  if (results.triangle == nullptr) {
    // if no collision is made, either all direct accumulation must be
    // discarded, or if there's an environment map, that can be used as an
    // emission source
    if (!scene.environmentTexture.Valid()) {
      return PropagationStatus::End;
    }
    glm::vec3 emissiveColor = Sample(scene.environmentTexture, wi);

    accumulatedIrradiance += radiance * emissiveColor;
    return PropagationStatus::DirectAccumulation;
  }

  auto const & mesh = scene.meshes[results.triangle->meshIdx];

  // -- apply direct emission
  if (mesh.material.emissive) {
    glm::vec3 emissiveColor = mesh.material.colorEmissive;

    accumulatedIrradiance += radiance * emissiveColor;
    return PropagationStatus::DirectAccumulation;
  }

  // generate bsdf sample (this will also be used for next propagation)
  auto [bsdfWo, bsdfPdf] = BsdfSample(pixelInfo, results, wi);

  // save previous radiance if necessary for future computations (ei NEE)
  auto previousRadiance = radiance;

  auto bsdffs =BsdfFs(scene, results, wi, bsdfWo);

  if (bsdfPdf != 0.0f) {
    radiance *= bsdffs * bsdfPdf ;
  }

  if (step == 1) {
    /* radiance *= 1000000.0f; */
    /* accumulatedIrradiance = glm::vec3(1.0f); */
    /* return PropagationStatus::DirectAccumulation; */
  }

  auto propagationStatus = PropagationStatus::Continue;

  /* { // -- apply next-event estimation */
  /*   auto [emissionTriangle, emissionBarycentricUvCoords] = */
  /*     EmissionSourceTriangle(scene, *pixelInfo.noise); */
  /*   auto emissionOrigin = */
  /*     BarycentricInterpolation( */
  /*       emissionTriangle->v0, emissionTriangle->v1, emissionTriangle->v2 */
  /*     , emissionBarycentricUvCoords */
  /*     ); */

  /*   glm::vec3 emissionWo = glm::normalize(emissionOrigin - ro); */

  /*   // check that emission source is within reflected hemisphere */
  /*   if (glm::dot(emissionWo, results.normal) > 0.0f) { */
  /*     auto [triangle, intersection] = */
  /*       Raycast(scene, ro + emissionWo*0.01f, emissionWo, useBvh); */

  /*     if (triangle == emissionTriangle) { */
  /*       propagationStatus = PropagationStatus::IndirectAccumulation; */
  /*       float surfaceArea = triangle->area(); */
  /*       float distance = glm::length(emissionOrigin - ro); */
  /*       float emitPdf = ( (surfaceArea*SQR(distance))); */
  /*       float bsdfPdf = BsdfPdf(results, wi, emissionWo); */
  /*       accumulatedIrradiance += */
  /*         scene.meshes[triangle->meshIdx].material.colorEmissive */
  /*       * BsdfFs(scene, results, wi, emissionWo) */
  /*       * previousRadiance */
  /*       / ((1.0f/bsdfPdf)/((1.0f/bsdfPdf) + emitPdf)) */
  /*       ; */
  /*       /1*   bsdfPdf, emitPdf,  (bsdfPdf/(bsdfPdf + emitPdf)) *1/ */
  /*       /1* ); *1/ */
  /*       /1*   scene.meshes[triangle->meshIdx].material.colorEmissive *1/ */
  /*       /1* , BsdfFs(scene, results, wi, emissionWo) *1/ */
  /*       /1* ); *1/ */

  /*       return PropagationStatus::DirectAccumulation; */
  /*     } */
  /*   } */
  /* } */

  { // apply next raycast
    auto [triangle, intersection] =
      Raycast(scene, ro + bsdfWo*0.01f, bsdfWo, useBvh);
    results = RaycastInfo::Construct(triangle, intersection, bsdfWo);

    wi = bsdfWo;
    ro += wi*results.intersection.length;
  }
  return propagationStatus;
}

} // -- end anon namespace

////////////////////////////////////////////////////////////////////////////////
struct RenderResults {
  RenderResults() = default;

  glm::vec3 color;
  bool valid;
};

////////////////////////////////////////////////////////////////////////////////
/*
  General order for how to apply light transport:
    * get camera/pixel info
    * apply initial raycast (that way we can apply special behaviour, like bg)
    * enter loop
      - call Progagate
        - get next ray direction sample
        - apply bsdf, shading, etc
        - perform raycast for next propagation
      - determine if loop should end or continue
*/
RenderResults Render(
  PixelInfo & pixelInfo
, Scene const & scene
, Camera const & camera
, uint32_t pathsPerSample
, bool useBvh
) {
  glm::vec3 wi;

  // -- apply dithering to pixel uv
  glm::vec2 ditherUv = pixelInfo.uv;
  ditherUv +=
    (glm::vec2(-0.5f) + SampleUniform2(*pixelInfo.noise))
  * (1.0f / pixelInfo.dimensions);

  // -- get initial incoming angle
  wi =
    LookAt(
      glm::normalize(camera.lookat - camera.ori)
    , ditherUv
    , camera.up
    , glm::radians(camera.fov)
    );

  // -- apply initial raycast
  RaycastInfo raycastResult;
  glm::vec3 origin;
  {
    auto [triangle, intersection] = Raycast(scene, camera.ori, wi, useBvh);
    raycastResult = RaycastInfo::Construct(triangle, intersection, wi);
    // important to update origin, otherwise origin would start at camera origin
    origin = camera.ori + wi*intersection.length;
  }

  // -- if nothing is hit just sample the skybox
  if (!raycastResult.triangle) {
    RenderResults renderResults;
    renderResults.color = glm::vec3(1.0f, 0.0f, 1.0f);
    if (scene.environmentTexture.Valid()) {
      renderResults.color = Sample(scene.environmentTexture, wi);
    }
    renderResults.valid = true;
    return renderResults;
  }

  // iterate light transport
  bool hit = false;
  glm::vec3
    radiance = glm::vec3(1.0f)
  , accumulatedIrradiance = glm::vec3(0.0f);

  for (size_t it = 0; it < pathsPerSample; ++ it) {
    PropagationStatus status =
      Propagate(
        pixelInfo
      , scene
      , raycastResult
      , radiance
      , accumulatedIrradiance
      , origin
      , wi
      , it
      , useBvh
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

  RenderResults renderResults;
  renderResults.color = accumulatedIrradiance;
  renderResults.valid = hit;
  return renderResults;
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 Render(
  glm::vec2 const uv
, glm::vec2 const dimensions
, GenericNoiseGenerator & noiseGenerator
, Scene const & scene
, Camera const & camera
, uint32_t samplesPerPixel
, uint32_t pathsPerSample
, bool useBvh
) {
  auto pixelInfo = PixelInfo{};
  pixelInfo.uv = uv;
  pixelInfo.dimensions = dimensions;
  pixelInfo.noise = &noiseGenerator;

  std::vector<glm::vec3> samples;
  samples.reserve(samplesPerPixel);

  // iterate collecting every sample per pixel; dependent on the scene, there
  // might be no pixel contribution on sample iterations, in which case the
  // calculation is ignored.
  // To help alleviate the chance of ignored samples, we make up to 2x the
  // attempts to collect samples
  for (uint32_t maxIt = 0; maxIt < samplesPerPixel*2.0f; ++ maxIt) {
    auto renderResults =
      Render(pixelInfo, scene, camera, pathsPerSample, useBvh);
    if (!renderResults.valid) { continue; }
    samples.emplace_back(renderResults.color);
    if (samples.size() >= samplesPerPixel) { break; }
  }

  // apply averaging to accumulated colors
  glm::vec3 accumulatedColor = glm::vec3(0.0f);
  for (auto & s : samples)
    { accumulatedColor += 1.0f/static_cast<float>(samples.size()) * s; }
  // I've heard that average should not be done by 1.0f/ but glm::Tau on
  // twitter, need to find out how true that is; i think it might depdend on
  // camera model anyways

  return accumulatedColor;
}
