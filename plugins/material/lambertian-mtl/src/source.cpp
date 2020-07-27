// lambertian material

#include <monte-toad/core/accelerationstructure.hpp>
#include <monte-toad/core/geometry.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/math.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <monte-toad/core/triangle.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace {

struct MaterialInfo {
  glm::vec3 albedo;

  mt::core::Texture * albedoTexture = nullptr;
};

} // -- namespace

extern "C" {

char const * PluginLabel() { return "lambertian mtl"; }
mt::PluginType PluginType() { return mt::PluginType::Material; }

void Load(mt::PluginInfoMaterial & self, mt::core::Scene & scene) {
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
}

glm::vec3 BsdfFs(
  mt::PluginInfoMaterial const & self
, mt::core::SurfaceInfo const & surface
, glm::vec3 const & wo
) {
  auto const & material =
    reinterpret_cast<MaterialInfo const *>(self.userdata)[surface.material];

  auto albedo = material.albedo;
  if (material.albedoTexture)
    { albedo = mt::core::Sample(*material.albedoTexture, surface.uvcoord); }

  return glm::dot(wo, surface.normal) * glm::InvPi * albedo;
}

float BsdfPdf(
  mt::PluginInfoMaterial const & /*self*/
, mt::core::SurfaceInfo const & surface
, glm::vec3 const & wo
) {
  return glm::max(0.0f, glm::InvPi * glm::dot(wo, surface.normal));
}

mt::BsdfSampleInfo BsdfSample(
  mt::PluginInfoMaterial const & self
, mt::PluginInfoRandom const & random
, mt::core::SurfaceInfo const & surface
) {
  glm::vec2 const u = random.SampleUniform2();

  glm::vec3 const wo =
    ReorientHemisphere(
      glm::normalize(Cartesian(glm::sqrt(u.y), glm::Tau*u.x))
    , surface.normal
    );

  float pdf = BsdfPdf(self, surface, wo);
  glm::vec3 fs = BsdfFs(self, surface, wo);
  return { wo, fs, pdf };
}

bool IsEmitter(
  mt::PluginInfoMaterial const & /*self*/
, mt::core::Triangle const & /*triangle*/
) {
  return false;
}

size_t currentMtlIdx = static_cast<size_t>(-1);

void UiUpdate(
  mt::core::Scene & scene
, mt::core::RenderInfo & render
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

    auto surface =
      mt::core::Raycast(scene, camera.origin, camera.direction, nullptr);

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

  // -- material editor (this is last of the function)
  if (!ImGui::Begin("Material Editor")) {}

  ImGui::Text("Selected idx %lu\n", currentMtlIdx);

  ImGui::Separator();

  if (currentMtlIdx == static_cast<size_t>(-1)) {
    ImGui::End();
    return;
  }

  auto & material =
    reinterpret_cast<MaterialInfo *>(plugin.material.userdata)[
      currentMtlIdx
    ];

  ImGui::PushID(std::to_string(currentMtlIdx).c_str());
  ImGui::Text("Mtl %lu", currentMtlIdx);

  ImGui::Separator();

  ImGui::Text("albedo");

  if (!material.albedoTexture) {
    if (ImGui::ColorPicker3("##albedo", &material.albedo.x)) {
      render.ClearImageBuffers();
    }
  }

  { // -- albedo texture
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

  ImGui::PopID();

  ImGui::End();
}

} // -- end extern "C"
