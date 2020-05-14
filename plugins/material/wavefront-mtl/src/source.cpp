#include <monte-toad/log.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <mt-plugin/plugin.hpp>

#include <cr/cr.h>
#include <imgui/imgui.hpp>

namespace {

struct MaterialInfo {
  glm::vec3 diffuse = glm::vec3(0.0f);
  float roughness = 1.0f;
  float emission = 0.0f;
};

void UpdateSceneEmission(mt::Scene & scene) {
  if (!scene.accelStructure) { return; }
  scene.emissionSource.triangles.resize(0);
  for (size_t i = 0; i < scene.accelStructure->triangles.size(); ++ i) {
    auto & material =
      std::any_cast<MaterialInfo &>(
        scene.meshes[scene.accelStructure->triangles[i].meshIdx].userdata
      );

    if (material.emission > 0.00001f)
      { scene.emissionSource.triangles.emplace_back(i); }
  }
}

void Load(mt::Scene & scene) {
  for (auto & mesh : scene.meshes)
    { mesh.userdata = std::make_any<MaterialInfo>(); }

  scene.emissionSource.triangles.clear();

  UpdateSceneEmission(scene);
}

std::tuple<glm::vec3 /*wo*/, float /*pdf*/> BsdfSample(
  mt::PluginInfoRandom & random
, mt::SurfaceInfo const & surface
, glm::vec3 const & wi
) {
  glm::vec3 wo; float pdf;

  wo = glm::normalize(glm::vec3(1.0f));
  pdf = 1.0f;

  return { wo, pdf };
}

float BsdfPdf(
  mt::SurfaceInfo const & surface
, glm::vec3 const & wi, glm::vec3 const & wo
) {
  return 1.0f;
}

glm::vec3 BsdfFs(
  mt::Scene const & scene, mt::SurfaceInfo const & surface
, glm::vec3 const & wi, glm::vec3 const & wo
) {
  auto const & material =
    std::any_cast<MaterialInfo const &>(
      scene.meshes[surface.triangle->meshIdx].userdata
    );

  return material.diffuse;
}

void UiUpdate(
  mt::Scene & scene
, mt::RenderInfo & render
, mt::PluginInfo & plugin
, mt::DiagnosticInfo & diagnosticInfo
) {
  static bool mtlOpen = false;
  static size_t currentMtlIdx = static_cast<size_t>(-1);

  ImGui::Begin("Material");

  for (size_t it = 0; it < scene.meshes.size(); ++ it) {
    if (ImGui::Button(fmt::format("Mtl {}", it).c_str())) {
      currentMtlIdx = it;
      mtlOpen = true;
    }
  }

  ImGui::End();

  // in case different scene loads or something else weird happens
  if (currentMtlIdx >= scene.meshes.size())
    { currentMtlIdx = static_cast<size_t>(-1); }

  if (
      mtlOpen && currentMtlIdx != static_cast<size_t>(-1)
   && ImGui::Begin("Material Editor", &mtlOpen)
   ) {
    auto & material =
      std::any_cast<MaterialInfo &>(scene.meshes[currentMtlIdx].userdata);
    ImGui::ColorPicker3("diffuse", &material.diffuse.x);
    ImGui::SliderFloat("roughness", &material.roughness, 0.0f, 1.0f);
    if (ImGui::SliderFloat("emission", &material.emission, 0.0f, 100.0f)) {
      UpdateSceneEmission(scene);
    }
  }
}

}

CR_EXPORT int cr_main(struct cr_plugin * ctx, enum cr_op operation) {
  // return immediately if an update, this won't do anything
  if (operation == CR_UNLOAD || operation == CR_STEP) { return 0; }
  if (!ctx || !ctx->userdata) { return 0; }

  auto & userInterface =
    *reinterpret_cast<mt::PluginInfoMaterial*>(ctx->userdata);

  switch (operation) {
    case CR_STEP: break;
    case CR_LOAD:
      userInterface.Load = &::Load;
      userInterface.BsdfSample = &::BsdfSample;
      userInterface.BsdfPdf = &::BsdfPdf;
      userInterface.BsdfFs = &::BsdfFs;
      userInterface.UiUpdate = &::UiUpdate;
      userInterface.pluginType = mt::PluginType::Material;
      spdlog::info("wave-front mtl plugin successfully loaded");
    break;
    case CR_UNLOAD: break;
    case CR_CLOSE: break;
  }

  return 0;
}
