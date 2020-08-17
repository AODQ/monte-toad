// lambertian material

#include <monte-toad/core/any.hpp>
#include <monte-toad/core/geometry.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/math.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/spectrum.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <monte-toad/core/triangle.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace {

struct MaterialInfo {
  mt::core::TextureOption<glm::vec3> albedo {"albedo"};
  mt::core::TextureOption<float> emission {"emission", 0.0f, 10.0f};
  bool albedoTextureLinearSpace = false;
};

void Deallocate(void * data) {
  delete reinterpret_cast<::MaterialInfo*>(data);
}

} // -- namespace

extern "C" {

char const * PluginLabel() { return "lambertian bsdf"; }
mt::PluginType PluginType() { return mt::PluginType::Bsdf; }

void Allocate(mt::core::Any & userdata) {
  userdata.Clear();
  userdata.data = new ::MaterialInfo{};
  userdata.dealloc = ::Deallocate;
}

glm::vec3 BsdfFs(
  mt::core::Any & userdata
, mt::core::SurfaceInfo const & surface
, glm::vec3 const & wo
) {
  auto & material = *reinterpret_cast<MaterialInfo const *>(userdata.data);

  auto albedo = material.albedo.Get(surface.uvcoord);
  albedo =
    glm::pow(
      albedo,
      glm::vec3(material.albedoTextureLinearSpace ? 1.0f : 2.2f)
    );

  float emission = material.emission.Get(surface.uvcoord);
  if (emission > 0.0f) { return emission * albedo; }

  return glm::dot(wo, surface.normal) * glm::InvPi * albedo;
}

glm::vec3 AlbedoApproximation(
  mt::core::Any const & userdata, float const /*indexOfRefraction*/
, mt::core::SurfaceInfo const & surface
) {
  auto & material = *reinterpret_cast<MaterialInfo const *>(userdata.data);

  auto albedo = material.albedo.Get(surface.uvcoord);
  albedo =
    glm::pow(
      albedo,
      glm::vec3(material.albedoTextureLinearSpace ? 1.0f : 2.2f)
    );

  float emission = material.emission.Get(surface.uvcoord);
  if (emission > 0.0f) { return emission * albedo; }

  return albedo;
}

float BsdfPdf(
  mt::core::Any & /*userdata*/
, mt::core::SurfaceInfo const & surface
, glm::vec3 const & wo
) {
  return glm::max(0.0f, glm::InvPi * glm::dot(wo, surface.normal));
}

mt::core::BsdfSampleInfo BsdfSample(
  mt::core::Any & userdata
, mt::PluginInfoRandom const & random
, mt::core::SurfaceInfo const & surface
) {
  glm::vec2 const u = random.SampleUniform2();

  glm::vec3 const wo =
    ReorientHemisphere(
      glm::normalize(Cartesian(glm::sqrt(u.y), glm::Tau*u.x))
    , surface.normal
    );

  float pdf = BsdfPdf(userdata, surface, wo);
  glm::vec3 fs = BsdfFs(userdata, surface, wo);
  return { wo, fs, pdf };
}

mt::BsdfTypeHint BsdfType() { return mt::BsdfTypeHint::Diffuse; }

bool IsEmitter(
  mt::core::Any const & userdata
, mt::core::Triangle const /*triangle*/
) {
  auto & material = *reinterpret_cast<::MaterialInfo*>(userdata.data);
  // Temporarily allow this material to be an emitter, though i should probably
  // not use this for that case
  return
      material.emission.userValue > 0.0f
   || material.emission.userTexture != nullptr
  ;
}

void UiUpdate(
  mt::core::Any & userdata
, mt::core::RenderInfo & render
, mt::core::Scene & scene
) {
  auto & material = *reinterpret_cast<::MaterialInfo*>(userdata.data);

  if (material.emission.GuiApply(scene))
    { render.ClearImageBuffers(); }

  if (material.albedo.GuiApply(scene))
    { render.ClearImageBuffers(); }

  if (ImGui::Checkbox("linear space", &material.albedoTextureLinearSpace))
    { render.ClearImageBuffers(); }
}

} // -- end extern "C"
