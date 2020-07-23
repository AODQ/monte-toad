// ggx material

#include <monte-toad/geometry.hpp>
#include <monte-toad/imgui.hpp>
#include <monte-toad/integratordata.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <mt-plugin/plugin.hpp>

namespace {

struct MaterialInfo {
  glm::vec3 fresnel = glm::vec3(1.0f);
  glm::vec3 albedo = glm::vec3(0.5f);
  glm::vec2 roughness = glm::vec2(1.0f);
  float emission = 0.0f;
};

// TODO TOAD this should be taken care of by an emitter plugin
void UpdateSceneEmission(
  mt::Scene & scene
, mt::PluginInfoMaterial const & self
) {
  if (!scene.accelStructure) { return; }
  scene.emissionSource.triangles.resize(0);
  for (size_t i = 0; i < scene.accelStructure->triangles.size(); ++ i) {
    auto & material =
      reinterpret_cast<MaterialInfo *>(self.userdata)[
        scene.accelStructure->triangles[i].meshIdx
      ];

    if (material.emission > 0.00001f)
      { scene.emissionSource.triangles.emplace_back(i); }
  }
}

} // -- namespace

extern "C" {

char const * PluginLabel() { return "wavefront mtl"; }
mt::PluginType PluginType() { return mt::PluginType::Material; }

void Load(mt::PluginInfoMaterial & self, mt::Scene & scene) {
  // free previous data
  if (self.userdata) { free(self.userdata); }

  // allocate data
  auto * mtl =
    reinterpret_cast<MaterialInfo*>(
      malloc(scene.meshes.size() * sizeof(MaterialInfo))
    );
  self.userdata = reinterpret_cast<void*>(mtl);

  // assign data
  for (size_t i = 0; i < scene.meshes.size(); ++ i) {
    mtl[i] = MaterialInfo{};
  }

  // TODO these should probably be emitter
  scene.emissionSource.triangles.clear();
  UpdateSceneEmission(scene, self);
}

float CosThetaSqr(glm::vec3 const & w) { return w.z*w.z; }
float CosTheta(glm::vec3 const & w) { return w.z; }

float SinThetaSqr(glm::vec3 const & w) {
  return glm::max(0.0f, 1.0f - CosThetaSqr(w));
}

float SinTheta(glm::vec3 const & w) { return glm::sqrt(SinThetaSqr(w)); }

float CosPhi(glm::vec3 const & w) {
  float const sinTheta = SinTheta(w);
  return sinTheta != 0.0f ? glm::clamp(w.x/sinTheta, -1.0f, +1.0f) : +1.0f;
}

float SinPhi(glm::vec3 const & w) {
  float const sinTheta = SinTheta(w);
  return sinTheta != 0.0f ? glm::clamp(w.y/sinTheta, -1.0f, +1.0f) : +0.0f;
}

float SinPhiSqr(glm::vec3 const & w) {
  const float sinPhi = SinPhi(w);
  return sinPhi*sinPhi;
}

float CosPhiSqr(glm::vec3 const & w) {
  const float cosPhi = CosPhi(w);
  return cosPhi*cosPhi;
}

float TanTheta(glm::vec3 const & w) { return SinTheta(w) / CosTheta(w); }

float TanThetaSqr(glm::vec3 const & w) {
  return SinThetaSqr(w) / CosThetaSqr(w);
}

float TrowbridgeReitzDistribution(glm::vec3 wh, glm::vec2 alpha) {
  float const tanThetaSqr = TanThetaSqr(wh);
  if (glm::isinf(tanThetaSqr)) { return 0.0f; }
  float const cosThetaPow4 = wh.z*wh.z*wh.z*wh.z;
  float const e =
    1.0f
  + (
      CosPhiSqr(wh) / (alpha.x*alpha.x)
    + SinPhiSqr(wh) / (alpha.y*alpha.y)
    )
  * tanThetaSqr
  ;

  return 1.0f/(glm::Pi * alpha.x*alpha.y * cosThetaPow4 * e*e);
}

float TrowbridgeReitzGeometric(glm::vec3 wh, glm::vec2 alpha) {
  float const tanThetaAbs = glm::abs(TanTheta(wh));
  if (glm::isinf(tanThetaAbs)) { return 0.0f; }
  float const alphaXy =
    glm::sqrt(
      CosPhiSqr(wh)*alpha.x*alpha.x
    + SinPhiSqr(wh)*alpha.y*alpha.y
    );
  return (-1.0f + glm::sqrt(1.0f + SQR(alphaXy*tanThetaAbs))) * 0.5f;
}

glm::vec2 TrowbridgeReitzSample11(float cosTheta, glm::vec2 u) {
  // special case (normal incidence)
  if (cosTheta > 0.9999f) {
    float const r = glm::sqrt(u.x / (1.0f - u.y));
    float const phi = glm::Tau * u.y;
    return r * glm::vec2(glm::cos(phi), glm::sin(phi));
  }

  float const
    sinTheta = glm::sqrt(glm::max(0.0f, 1.0f - cosTheta*cosTheta))
  , tanTheta = sinTheta/cosTheta
  , g1_a     = 1.0f/tanTheta
  , g1       = 2.0f/(1.0f + glm::sqrt(1.0f + 1.0f/(g1_a*g1_a)))
  ;

  glm::vec2 slope;

  { // sample slope.x
    float const
      a = 2.0f * u.x/g1 - 1.0f
    , tmp = glm::min(1e10f, 1.0f/(a*a - 1.0f))
    , b = tanTheta
    , d = glm::sqrt(glm::max(0.0f, b*b*tmp*tmp - (a*a - b*b)*tmp))
    , slopex0 = b*tmp - d
    , slopex1 = b*tmp + d
    ;
    slope.x = (a < 0 || slopex1 > 1.0f/tanTheta) ? slopex1 : slopex0;
  }

  { // sample slope.y
    float s;
    if (u.y > 0.5f) {
      s = +1.0f;
      u.y = 2.0f*(u.y - 0.5f);
    } else {
      s = -1.0f;
      u.y = 2.0f*(0.5f - u.y);
    }
    float const z =
      (u.y*(u.y*(u.y*0.27385f  - 0.73369f) + 0.46341f))
    / (u.y*(u.y*(u.y*0.093073f + 0.30942f) - 1.00000f) + 0.597999f)
    ;
    slope.y = s*z*glm::sqrt(1.0f + slope.x*slope.x);
  }

  return slope;
}

glm::vec3 TrowbridgeReitzSample(
  glm::vec3 const & wi, glm::vec2 alpha, glm::vec2 u
) {
  // strech ωᵢ
  auto wiStretch = glm::normalize(glm::vec3(alpha, 1.0f) * wi);

  // simulate P₂₂ωᵢ(xΔ, yΔ, 1, 1)
  glm::vec2 slope = TrowbridgeReitzSample11(CosTheta(wiStretch), u);

  { // rotate
    const float
      wiStretchCosPhi = CosPhi(wiStretch)
    , wiStretchSinPhi = SinPhi(wiStretch)
    ;
    float tmp = wiStretchCosPhi*slope.x - wiStretchSinPhi*slope.y;
    slope.y = wiStretchSinPhi*slope.x + wiStretchCosPhi*slope.y;
    slope.x = tmp;
  }

  // normalize
  return glm::normalize(glm::vec3(-slope.x, -slope.y, 1.0f));
}

glm::vec3 FaceForward(glm::vec3 n, glm::vec3 v) {
  return glm::dot(n, v) < 0.0f ? -n : n;
}

glm::vec3 BsdfFs(
  mt::PluginInfoMaterial const & self
, mt::SurfaceInfo const & surface
, glm::vec3 const & wo
) {
  auto const
    wi = -surface.incomingAngle
  , wh = glm::normalize(wi + wo)
  ;
  float const
    cosThetaO = glm::abs(::CosTheta(wo))
  , cosThetaI = glm::abs(::CosTheta(wi))
  ;

  // denegerate cases
  /* if (cosThetaI == 0.0f || cosThetaO == 0.0f) { return glm::vec3(0.0f); } */
  /* if (wh == glm::vec3(0.0f)) { return glm::vec3(0.0f); } */

  auto const & material =
    reinterpret_cast<MaterialInfo const *>(self.userdata)[surface.material];

  // for fresnel call, make sure wh is in same hemisphere as surface normal for
  // TIR
  glm::vec3 const F =
    glm::vec3(1.0f - material.fresnel)
  * glm::pow(1.0f - FaceForward(wh, glm::vec3(0, 0,1)), glm::vec3(5.0f));
  float const D = TrowbridgeReitzDistribution(wh, material.roughness);
  float const G =
    1.0f
  / (
      1.0f
    + TrowbridgeReitzGeometric(wo, material.roughness)
    + TrowbridgeReitzGeometric(wi, material.roughness)
    );

  return glm::vec3(0.5f);
  return glm::vec3(cosThetaO);
  return
    material.albedo * D * G * F / (1.0f * cosThetaI * cosThetaO);
}


float BsdfPdf(
  mt::PluginInfoMaterial const & /*self*/
, mt::SurfaceInfo const & surface
, glm::vec3 const & wo
) {
  /* auto const & material = */
  /*   reinterpret_cast<MaterialInfo const *>(self.userdata)[surface.material]; */

  return glm::max(0.0f, glm::InvPi * glm::dot(wo, surface.normal));

  /* glm::vec3 wh = glm::normalize(-surface.incomingAngle + wo); */
  /* float const distributionPdf = */
  /*   TrowbridgeReitzDistribution(wh, material.roughness) */
  /* * TrowbridgeReitzGeometric(wo, material.roughness) */
  /* * glm::abs(glm::dot(wo, wh)) / glm::abs(CosTheta(wo)) */
  /* ; */

  /* return distributionPdf / (4.0f * glm::dot(wh, wo)); */
}

bool SameHemisphere(glm::vec3 wi, glm::vec3 wo) {
  return wi.z*wo.z > 0.0f;
}

mt::BsdfSampleInfo BsdfSample(
  mt::PluginInfoMaterial const & self
, mt::PluginInfoRandom const & random
, mt::SurfaceInfo const & surface
) {
  auto const & material =
    reinterpret_cast<MaterialInfo const *>(self.userdata)[surface.material];

  glm::vec2 const u = random.SampleUniform2();

  glm::vec3 const wo =
    ReorientHemisphere(
      glm::normalize(Cartesian(glm::sqrt(u.y), glm::Tau*u.x))
    , surface.normal
    );

  float pdf = BsdfPdf(self, surface, wo);
  glm::vec3 fs = material.albedo * glm::dot(surface.normal, wo) * glm::InvPi;
  return { wo, fs, pdf };

  /* glm::vec3 const */
  /*   wi = -surface.incomingAngle */
  /* , wh = TrowbridgeReitzSample(wi, material.roughness, random.SampleUniform2()) */
  /* ; */

  /* // rare */
  /* if (glm::dot(wi, wh) < 0.0f) { return {glm::vec3(0), glm::vec3(0), 0}; } */

  /* /1* auto wo = glm::reflect(wi, wh); *1/ */
  /* auto wo = ReorientHemisphere(glm::normalize(wh), surface.normal); */

  /* if (!SameHemisphere(wi, wo)) { return {glm::vec3(0), glm::vec3(0), 0}; } */

  /* auto pdf = BsdfPdf(self, surface, wo); */
  /* auto bsdfFs = ::BsdfFs(self, surface, wo); */

  /* return { wo, bsdfFs, pdf }; */
}

bool IsEmitter(
  mt::PluginInfoMaterial const & /*self*/
, mt::Triangle const & /*triangle*/
) {
  return false;
  /* auto const & mtl = */
  /*   reinterpret_cast<MaterialInfo const *>(self.userdata)[triangle.meshIdx]; */
  /* return mtl.emission > 0.0f; */
}

size_t currentMtlIdx = static_cast<size_t>(-1);

void UiUpdate(
  mt::Scene & scene
, mt::RenderInfo & render
, mt::PluginInfo const & plugin
) {

  // -- if an image is clicked then update where it was clicked
  if (render.lastIntegratorImageClicked != -1lu) {
    auto & data = render.integratorData[render.lastIntegratorImageClicked];
    auto uv =
      glm::vec2(data.imagePixelClickedCoord)
    / glm::vec2(data.imageResolution);

    uv = (uv - glm::vec2(0.5f)) * 2.0f;
    uv.y *=
      data.imageResolution[1] / static_cast<float>(data.imageResolution[0]);

    auto camera =
      plugin.camera.Dispatch(
        plugin.random, render.camera, data.imageResolution, uv
      );

    auto surface = mt::Raycast(scene, camera.origin, camera.direction, nullptr);

    currentMtlIdx =
      static_cast<size_t>(surface.Valid() ? surface.triangle->meshIdx : -1);

    // TODO move below elsewhere
    render.lastIntegratorImageClicked = -1lu;
  }

  if (ImGui::Begin("Material")) {

    ImGui::Text("Emitters %lu", scene.emissionSource.triangles.size());

    ImGui::Separator();
    ImGui::Text("-- materials --");
    if (ImGui::Button("-")) {
      currentMtlIdx = (currentMtlIdx-1) % scene.meshes.size();
    }
    ImGui::SameLine();
    if (ImGui::Button("+")) {
      currentMtlIdx = (currentMtlIdx+1) % scene.meshes.size();
    }
    for (size_t it = 0; it < scene.meshes.size(); ++ it) {
      if (ImGui::Button(fmt::format("Mtl {}", it).c_str())) {
        currentMtlIdx = it;
      }
    }

    ImGui::End();
  }

  // in case different scene loads or something else weird happens
  if (currentMtlIdx >= scene.meshes.size())
    { currentMtlIdx = static_cast<size_t>(-1); }

  if (ImGui::Begin("Material Editor")) {

    ImGui::Text("Selected idx %lu\n", currentMtlIdx);

    if (currentMtlIdx != static_cast<size_t>(-1)) {
      auto & material =
        reinterpret_cast<MaterialInfo *>(plugin.material.userdata)[
          currentMtlIdx
        ];
      ImGui::PushID(std::to_string(currentMtlIdx).c_str());
      ImGui::Text("Mtl %lu", currentMtlIdx);
      if (ImGui::InputFloat("emission", &material.emission)) {
        render.ClearImageBuffers();
      }
      if (ImGui::ColorPicker3("albedo", &material.albedo.x)) {
        render.ClearImageBuffers();
      }
      if (ImGui::ColorPicker3("fresnel", &material.fresnel.x)) {
        render.ClearImageBuffers();
      }
      if (ImGui::SliderFloat2("roughness", &material.roughness.x, 0.0f, 1.0f)) {
        render.ClearImageBuffers();
      }
      ImGui::PopID();
    }

    ImGui::End();
  }
}

} // -- end extern "C"
