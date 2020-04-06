#include "render.hpp"

#include "primitive.hpp"
#include "scene.hpp"

#include <random>

namespace {
////////////////////////////////////////////////////////////////////////////////
template <typename U>
U BarycentricInterpolation(
  U const & v0, U const & v1, U const & v2
, glm::vec2 const & uv
) {
  return v0 + uv.x*(v1 - v0) + uv.y*(v2 - v0);
}

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

  std::random_device device;
  std::mt19937 generator { device() };
  std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
};

////////////////////////////////////////////////////////////////////////////////
struct RaycastInfo {
  Triangle const * triangle;
  Intersection intersection;
  glm::vec3 normal;
};

////////////////////////////////////////////////////////////////////////////////
float SampleUniform(PixelInfo const & pi) {
  return pi.distribution(pi.generator);
}

////////////////////////////////////////////////////////////////////////////////
glm::vec2 SampleUniform2(PixelInfo const & pi) {
  return glm::vec2(SampleUniform(pi), SampleUniform(pi));
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 SampleUniform3(PixelInfo const & pi) {
  return glm::vec3(SampleUniform(pi), SampleUniform(pi), SampleUniform(pi));
}

///////////////////////////////////////////////////////////////////////////////u/
std::pair<glm::vec3, glm::vec3> CalculateXY(glm::vec3 const & normal) {
  glm::vec3 binormal = glm::vec3(1.0f, 0.0f, 0.0f); // TODO swap if eq to normal
  binormal = glm::normalize(glm::cros(normal, binormal));
  bitangent = glm::cross(binormal, normal);
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
float BsdfPdf(RaycastInfo const & raycastInfo , glm::vec3 wi , glm::vec3 wo) {
  return 1.0f/3.14159f;
}

////////////////////////////////////////////////////////////////////////////////
std::pair<glm::vec3 /*wo*/, float /*pdf*/> BsdfSample(
  PixelInfo const & pixelInfo
, RaycastInfo const & raycastInfo
, glm::vec3 wi
) {
  glm::vec2 u = SampleUniform2(pixelInfo);
  glm::vec3 wo =
    ReorientHemisphere(
      glm::normalize(ToCartesian(glm::sqrt(u.y), Tau*u.x))
    );
  return std::make_pair(wo, BsdfPdf(raycastInfo, wi, wo));
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 BsdfFs(RaycastInfo const & raycastInfo, glm::vec3 wi, glm::vec3 wo) {
  return glm::dot(raycastInfo.normal, wo) * glm::vec3(0.2f);
}

////////////////////////////////////////////////////////////////////////////////
enum class PropagationStatus {
  Continue
, End
, IndirectAccumulation
, DirectAccumulation
};

PropagationStatus Propagate(
  PixelInfo const & pixelInfo
, RaycastInfo raycastResults
, glm::vec3 & radiance
, glm::vec3 & accumulatedIrradiance
, glm::vec3 & ro
, glm::vec3 & rd
, uint16_t step
) {
  if (raycastResults.first == nullptr) { return PropagationStatus::End; }

  glm::vec3 rayWo = BsdfSample(pixelInfo, raycastInfo, rd);
  // TODO

}

} // -- end anon namespace

////////////////////////////////////////////////////////////////////////////////
glm::vec3 Render(
  glm::vec2 const uv
, Scene const & scene
, Camera const & camera
, bool useBvh
) {
  glm::vec3 eyeOri, eyeDir;
  eyeOri = camera.ori;
  eyeDir =
    LookAt(
      glm::normalize(camera.lookat - camera.ori)
    , uv
    , camera.up
    , glm::radians(camera.fov)
    );

  glm::vec3 absorbedColor = glm::vec3(1.0f);

  Intersection intersection;
  Triangle const * triangle =
    Raycast(scene, eyeOri, eyeDir, intersection, useBvh);

  if (!triangle) {
    return
      scene.environmentTexture.Valid()
    ? Sample(scene.environmentTexture, eyeDir)
    : glm::vec3(0.2f, 0.2f, 0.2f)
    ;
  }

  glm::vec3 normal =
    glm::normalize(
      BarycentricInterpolation(
        triangle->n0, triangle->n1, triangle->n2
      , intersection.barycentricUv
      )
    );
  glm::vec2 uvcoord =
    BarycentricInterpolation(
      triangle->uv0, triangle->uv1, triangle->uv2
    , intersection.barycentricUv
    );
  glm::vec3 ori = eyeOri + eyeDir*intersection.distance;
    (void)ori;

  glm::vec3 diffuseTex = glm::vec3(0.2f);
  if (scene.meshes[triangle->meshIdx].diffuseTextureIdx
   != static_cast<size_t>(-1)
  ) {
    diffuseTex =
      SampleBilinear(
        scene.textures[scene.meshes[triangle->meshIdx].diffuseTextureIdx]
      , uvcoord
      );
  }

  glm::vec3 reflColor = glm::vec3(1.0f);

  if (scene.environmentTexture.Valid()) {
    reflColor =
      Sample(scene.environmentTexture, glm::reflect(-eyeDir, normal));
  }

  return
    /* normal */
    diffuseTex
  * reflColor
  /* * ( */
    /* glm::vec3(0.1f) */
  /* + glm::dot(normal, glm::normalize(glm::vec3(0.5f, 0.5f, -0.5f)))*0.9f */
  /* ) */
  ;
}
