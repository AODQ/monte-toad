// wavefront mtl

#include <monte-toad/imgui.hpp>
#include <monte-toad/integratordata.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/material/layered.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <mt-plugin/plugin.hpp>

namespace {

struct MaterialInfo {
  glm::vec3 diffuse = glm::vec3(0.5f);
  float roughness = 1.0f;
  float emission = 0.0f;
  float transmittive = 0.0f;
  float indexOfRefraction = 1.0f;
};

// TODO TOAD this should be taken care of by an emitter plugin
void UpdateSceneEmission(
  mt::Scene & scene
, mt::PluginInfoMaterial const & /*self*/
) {
  scene.emissionSource.triangles.resize(0);
  /* for (size_t i = 0; i < scene.accelStructure->triangles.size(); ++ i) { */
  /*   auto & material = */
  /*     reinterpret_cast<MaterialInfo *>(self.userdata)[ */
  /*       scene.accelStructure->triangles[i].meshIdx */
  /*     ]; */

  /*   if (material.emission > 0.00001f) */
  /*     { scene.emissionSource.triangles.emplace_back(i); } */
  /* } */
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

float BsdfPdf(
  mt::PluginInfoMaterial const & /*self*/
, mt::SurfaceInfo const & /*surface*/
, glm::vec3 const & /*wo*/
) {
  return 0.0f;
  /* return mt::material::layered::BsdfPdf(data, surface, wo); */
  /* auto const & material = */
  /*   reinterpret_cast<MaterialInfo const *>(self.userdata)[surface.material]; */

  /* if (material.transmittive > 0.0f) { return 0.0f; } */
  /* return glm::max(0.0f, glm::InvPi * glm::dot(wo, surface.normal)); */
}

mt::BsdfSampleInfo BsdfSample(
  mt::PluginInfoMaterial const & /*self*/
, mt::PluginInfoRandom const & /*random*/
, mt::SurfaceInfo const & /*surface*/
) {
  /* auto [wo, bsdfFs, pdf] = */
  /*   mt::material::layered::BsdfSample(data, random, surface); */

  /* return { wo, bsdfFs, pdf }; */
  return { glm::vec3(0.0f), glm::vec3(0.0f), 0.0f };
  /* glm::vec3 wo; */

  /* auto const & material = */
  /*   reinterpret_cast<MaterialInfo const *>(self.userdata)[surface.material]; */

  /* if (material.transmittive > 0.0f) { */

  /*   glm::vec3 normal = surface.normal; */
  /*   float eta = material.indexOfRefraction; */
  /*   float f0 = glm::sqr((1.0f - eta) /  (1.0f + eta)); */

  /*   // flip normal if surface is incorrect for refraction */
  /*   if (glm::dot(surface.incomingAngle, surface.normal) > 0.0f) { */
  /*     normal = -surface.normal; */
  /*     eta = 1.0f/eta; */
  /*   } */

  /*   float cosTheta = glm::dot(surface.incomingAngle, surface.normal); */
  /*   float fr = f0 + (1.0f - f0)*glm::pow(1.0f - cosTheta, 5.0f); */

  /*   bool reflection = random.SampleUniform1() > fr; */

  /*   // check for total internal reflection as well */
  /*   if (1.0f - eta*eta*(1.0f - cosTheta*cosTheta) < 0.0f) { */
  /*     reflection = true; */
  /*   } */

  /*   return */
  /*     { */
  /*       glm::normalize( */
  /*         reflection */
  /*       ? glm::reflect(surface.incomingAngle, -normal) */
  /*       : glm::refract( */
  /*           surface.incomingAngle */
  /*         , normal */
  /*         , surface.exitting */
  /*           ? material.indexOfRefraction */
  /*           : 1.0f/material.indexOfRefraction */
  /*         ) */
  /*       ), */
  /*       0.0f */
  /*     }; */
  /* } */

  /* glm::vec2 u = random.SampleUniform2(); */

  /* wo = */
  /*   ReorientHemisphere( */
  /*     glm::normalize(Cartesian(glm::sqrt(u.y), glm::Tau*u.x)) */
  /*   , surface.normal */
  /*   ); */

  /* return { wo, BsdfPdf(self, surface, wo) }; */
}

bool IsEmitter(
  mt::PluginInfoMaterial const & self
, mt::Scene const & /*scene*/
, mt::Triangle const & triangle
) {
  auto const & mtl =
    reinterpret_cast<MaterialInfo const *>(self.userdata)[triangle.meshIdx];
  return mtl.emission > 0.0f;
}

glm::vec3 BsdfFs(
  mt::PluginInfoMaterial const & /*self*/
, mt::SurfaceInfo const & /*surface*/
, glm::vec3 const & /*wo*/
) {
  return glm::vec3(0.0f);
  /* return mt::material::layered::BsdfFs(data, surface, wo); */
  /* auto const & material = */
  /*   reinterpret_cast<MaterialInfo const *>(self.userdata)[surface.material]; */

  /* if (material.emission > 0.0f) { return material.emission * material.diffuse; } */

  /* if (material.transmittive > 0.0f) { */
  /*   return material.diffuse; */
  /* } */

  /* return material.diffuse * dot(surface.normal, wo) * glm::InvPi; */
}

static size_t currentMtlIdx = static_cast<size_t>(-1);

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
        mt::Raycast(scene, camera.origin, camera.direction, nullptr);

      currentMtlIdx =
        static_cast<size_t>(surface.Valid() ? surface.triangle->meshIdx : -1);

      // TODO move below elsewhere
      render.lastIntegratorImageClicked = -1lu;
    }

    ImGui::End();
  }

  // in case different scene loads or something else weird happens
  if (currentMtlIdx >= scene.meshes.size())
    { currentMtlIdx = static_cast<size_t>(-1); }

  /* if (ImGui::Begin("Material Editor")) { */

  /*   if (ImGui::SliderFloat("Depth",  &data.layers[0].depth, 0.0f, 1.0f)) { */
  /*     render.ClearImageBuffers(); */
  /*   } */
  /*   if (ImGui::SliderFloat("sigmaA", &data.layers[0].sigmaA, 0.0f, 1.0f)) { */
  /*     render.ClearImageBuffers(); */
  /*   } */
  /*   if (ImGui::SliderFloat("sigmaS", &data.layers[0].sigmaS, 0.0f, 1.0f)) { */
  /*     render.ClearImageBuffers(); */
  /*   } */
  /*   if (ImGui::SliderFloat("g",      &data.layers[0].g, -2.0f, 2.0f)) { */
  /*     render.ClearImageBuffers(); */
  /*   } */
  /*   if (ImGui::SliderFloat("alpha",  &data.layers[0].alpha, 0.0f, 2.0f)) { */
  /*     render.ClearImageBuffers(); */
  /*   } */
  /*   if (ImGui::SliderFloat3("ior", &data.layers[0].ior.x, 0.0f, 1.0f)) { */
  /*     render.ClearImageBuffers(); */
  /*   } */
  /*   if (ImGui::SliderFloat3("kappa", &data.layers[0].kappa.x, 0.0f, 1.0f)) { */
  /*     render.ClearImageBuffers(); */
  /*   } */

  /*   if ( */
  /*     ImGui::SliderFloat3( */
  /*       "fresnel", &data.fresnelTable.fresnelConstant.x, 0.0f, 1.0f */
  /*     ) */
  /*   ) { */
  /*     render.ClearImageBuffers(); */
  /*   } */

  /*   if ( */
  /*     ImGui::SliderFloat( */
  /*       "total internal reflection", &data.tirTable.tirConstant, 0.0f, 1.0f */
  /*     ) */
  /*   ) { */
  /*     render.ClearImageBuffers(); */
  /*   } */

  /*   /1* if (currentMtlIdx != static_cast<size_t>(-1)) { *1/ */
  /*   /1*   auto & material = *1/ */
  /*   /1*     reinterpret_cast<MaterialInfo *>(plugin.material.userdata)[ *1/ */
  /*   /1*       currentMtlIdx *1/ */
  /*   /1*     ]; *1/ */
  /*   /1*   ImGui::PushID(std::to_string(currentMtlIdx).c_str()); *1/ */
  /*   /1*   ImGui::Text("Mtl %lu", currentMtlIdx); *1/ */
  /*   /1*   if (ImGui::ColorPicker3("diffuse", &material.diffuse.x)) { *1/ */
  /*   /1*     render.ClearImageBuffers(); *1/ */
  /*   /1*   } *1/ */
  /*   /1*   if (ImGui::SliderFloat("roughness", &material.roughness, 0.0f, 1.0f)) { *1/ */
  /*   /1*     render.ClearImageBuffers(); *1/ */
  /*   /1*   } *1/ */
  /*   /1*   if (ImGui::InputFloat("emission", &material.emission)) { *1/ */
  /*   /1*     UpdateSceneEmission(scene, plugin.material); *1/ */
  /*   /1*     render.ClearImageBuffers(); *1/ */
  /*   /1*   } *1/ */
  /*   /1*   if ( *1/ */
  /*   /1*       ImGui::InputFloat("transmittive", &material.transmittive) *1/ */
  /*   /1*    || ImGui::InputFloat("ior", &material.indexOfRefraction) *1/ */
  /*   /1*   ) { *1/ */
  /*   /1*     render.ClearImageBuffers(); *1/ */
  /*   /1*   } *1/ */
  /*   /1*   ImGui::PopID(); *1/ */
  /*   /1* } *1/ */

  /*   ImGui::End(); */
  /* } */
}

} // -- end extern "C"
