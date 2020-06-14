#include <cr/cr.h>

#include <monte-toad/geometry.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace {

struct MaterialInfo {
  glm::vec3 diffuse = glm::vec3(0.5f);
  float roughness = 1.0f;
  float emission = 0.0f;
  float transmittive = 0.0f;
  float indexOfRefraction = 1.0f;
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

float BsdfPdf(mt::SurfaceInfo const & surface, glm::vec3 const & wo) {
  auto const & material = std::any_cast<MaterialInfo const &>(surface.material);
  if (material.transmittive > 0.0f) { return 0.0f; }
  return glm::max(0.0f, glm::InvPi * glm::dot(wo, surface.normal));
}

std::tuple<glm::vec3 /*wo*/, float /*pdf*/> BsdfSample(
  mt::PluginInfoRandom const & random
, mt::SurfaceInfo const & surface
) {
  glm::vec3 wo;

  auto const & material = std::any_cast<MaterialInfo const &>(surface.material);

  if (material.transmittive > 0.0f) {

    glm::vec3 normal = surface.normal;

    // flip normal if surface is incorrect for refraction
    if (glm::dot(surface.incomingAngle, surface.normal) > 0.0f) {
      normal = -surface.normal;
    }

    bool reflection = random.SampleUniform1() > material.transmittive;

    return
      {
        glm::normalize(
          reflection
        ? glm::reflect(surface.incomingAngle, -normal)
        : glm::refract(
            surface.incomingAngle
          , normal
          , surface.exitting
            ? material.indexOfRefraction
            : 1.0f/material.indexOfRefraction
          )
        ),
        0.0f
      };
  }

  glm::vec2 u = random.SampleUniform2();

  wo =
    ReorientHemisphere(
      glm::normalize(Cartesian(glm::sqrt(u.y), glm::Tau*u.x))
    , surface.normal
    );

  return { wo, BsdfPdf(surface, wo) };
}

bool IsEmitter(mt::Scene const & scene, mt::Triangle const & triangle) {
  return
    std::any_cast<MaterialInfo const &>(
      scene.meshes[triangle.meshIdx].userdata
    ).emission > 0.0f;
}

glm::vec3 BsdfFs(
  mt::Scene const & scene, mt::SurfaceInfo const & surface, glm::vec3 const & wo
) {
  auto const & material = std::any_cast<MaterialInfo const &>(surface.material);

  if (material.emission > 0.0f) { return material.emission * material.diffuse; }

  if (material.transmittive > 0.0f) {
    return material.diffuse;
  }

  return material.diffuse * dot(surface.normal, wo) * glm::InvPi;
}

static CR_STATE size_t currentMtlIdx = static_cast<size_t>(-1);

void UiUpdate(
  mt::Scene & scene
, mt::RenderInfo & render
, mt::PluginInfo const & plugin
) {
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

    // TODO
    if (render.lastIntegratorImageClicked != -1) {
      auto & data = render.integratorData[render.lastIntegratorImageClicked];
      auto uv =
        glm::vec2(data.imagePixelClickedCoord)
      / glm::vec2(data.imageResolution);

      uv = (uv - glm::vec2(0.5f)) * 2.0f;
      uv.y *=
        data.imageResolution[1] / static_cast<float>(data.imageResolution[0]);

      auto [origin, wi] =
        plugin.camera.Dispatch(
          plugin.random, render, data.imageResolution, uv
        );

      auto surface = mt::Raycast(scene, origin, wi, nullptr);

      currentMtlIdx =
        static_cast<size_t>(surface.Valid() ? surface.triangle->meshIdx : -1);

      // TODO move below elsewhere
      render.lastIntegratorImageClicked = -1;
    }

    ImGui::End();
  }

  // in case different scene loads or something else weird happens
  if (currentMtlIdx >= scene.meshes.size())
    { currentMtlIdx = static_cast<size_t>(-1); }

  if (ImGui::Begin("Material Editor")) {

    if (currentMtlIdx != static_cast<size_t>(-1)) {
      auto & material =
        std::any_cast<MaterialInfo &>(scene.meshes[currentMtlIdx].userdata);
      ImGui::PushID(std::to_string(currentMtlIdx).c_str());
      ImGui::Text("Mtl %lu", currentMtlIdx);
      if (ImGui::ColorPicker3("diffuse", &material.diffuse.x)) {
        render.ClearImageBuffers();
      }
      if (ImGui::SliderFloat("roughness", &material.roughness, 0.0f, 1.0f)) {
        render.ClearImageBuffers();
      }
      if (ImGui::InputFloat("emission", &material.emission)) {
        UpdateSceneEmission(scene);
        render.ClearImageBuffers();
      }
      if (
          ImGui::InputFloat("transmittive", &material.transmittive)
       || ImGui::InputFloat("ior", &material.indexOfRefraction)
      ) {
        render.ClearImageBuffers();
      }
      ImGui::PopID();
    }

    ImGui::End();
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
      userInterface.IsEmitter = &::IsEmitter;
      userInterface.UiUpdate = &::UiUpdate;
      userInterface.pluginType = mt::PluginType::Material;
      userInterface.pluginLabel = "wavefront material";
    break;
    case CR_UNLOAD: break;
    case CR_CLOSE: break;
  }

  return 0;
}
