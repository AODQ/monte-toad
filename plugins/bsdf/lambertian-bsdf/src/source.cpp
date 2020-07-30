// lambertian material

#include <monte-toad/core/accelerationstructure.hpp>
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
  glm::vec3 albedo;

  float emission = 0.0f;
  mt::core::Texture * albedoTexture = nullptr;
  bool albedoTextureLinearSpace = false;
};

} // -- namespace

extern "C" {

char const * PluginLabel() { return "lambertian mtl"; }
mt::PluginType PluginType() { return mt::PluginType::Bsdf; }

void Allocate(mt::core::Any & userdata) {
  if (userdata.data == nullptr) { free(userdata.data); }
  userdata.data = ::malloc(sizeof(MaterialInfo));
  *reinterpret_cast<MaterialInfo*>(userdata.data) = {};
}

glm::vec3 BsdfFs(
  mt::core::Any & userdata
, mt::core::SurfaceInfo const & surface
, glm::vec3 const & wo
) {
  auto & material = *reinterpret_cast<MaterialInfo const *>(userdata.data);

  auto albedo = material.albedo;
  if (material.albedoTexture) {
    albedo = mt::core::Sample(*material.albedoTexture, surface.uvcoord);
    albedo =
      glm::pow(
        albedo,
        glm::vec3(material.albedoTextureLinearSpace ? 1.0f : 2.2f)
      );
  }

  if (material.emission > 0.0f) { return material.emission * albedo; }

  return glm::dot(wo, surface.normal) * glm::InvPi * albedo;
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

bool IsReflective() { return true; }
bool IsRefractive() { return false; }

bool IsEmitter(
  mt::core::Any const & userdata
, mt::core::Triangle const & /*triangle*/
) {
  auto & material = *reinterpret_cast<::MaterialInfo*>(userdata.data);
  // Temporarily allow this material to be an emitter, though i should probably
  // not use this for that case
  return material.emission > 0.0f;
}

void UiUpdate(
  mt::core::Any & userdata
, mt::core::RenderInfo & render
, mt::core::Scene & scene
) {
  auto & material = *reinterpret_cast<::MaterialInfo*>(userdata.data);

  if (ImGui::SliderFloat("emission", &material.emission, 0.0f, 15.0f)) {
    render.ClearImageBuffers();
  }

  if (!material.albedoTexture) {
    if (ImGui::ColorPicker3("##albedo", &material.albedo.x))
      { render.ClearImageBuffers(); }
  } else {
    if (ImGui::Checkbox("Linear Space", &material.albedoTextureLinearSpace))
      { render.ClearImageBuffers(); }
  }

  // -- albedo texture
  if (
    ImGui::BeginCombo(
      "Texture"
    , material.albedoTexture ? material.albedoTexture->label.c_str() : "none"
    )
  ) {
    if (ImGui::Selectable("none", !material.albedoTexture)) {
      material.albedoTexture = nullptr;
      render.ClearImageBuffers();
    }

    for (auto & tex : scene.textures) {
      if (
        ImGui::Selectable(
          tex.label.c_str()
        , &tex == material.albedoTexture
        )
      ) {
        material.albedoTexture = &tex;
        render.ClearImageBuffers();
      }
    }
    ImGui::EndCombo();
  }
}

} // -- end extern "C"
