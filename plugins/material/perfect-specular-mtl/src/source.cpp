// perfect specular material

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
};

} // -- namespace

extern "C" {

char const * PluginLabel() { return "perfect specular mtl"; }
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
, glm::vec3 const &
) {
  auto const & material =
    reinterpret_cast<MaterialInfo const *>(self.userdata)[surface.material];

  return material.albedo;
}

float BsdfPdf(
  mt::PluginInfoMaterial const & /*self*/
, mt::core::SurfaceInfo const & /*surface*/
, glm::vec3 const & /*wo*/
) {
  return 0.0f; // dirac delta
}

mt::BsdfSampleInfo BsdfSample(
  mt::PluginInfoMaterial const & self
, mt::PluginInfoRandom const & /*random*/
, mt::core::SurfaceInfo const & surface
) {
  glm::vec3 const wo =
    glm::normalize(glm::reflect(surface.incomingAngle, surface.normal));

  float pdf = 0.0f; // dirac delta
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

  if (ImGui::ColorPicker3("##albedo", &material.albedo.x)) {
    render.ClearImageBuffers();
  }

  ImGui::PopID();

  ImGui::End();
}

} // -- end extern "C"
