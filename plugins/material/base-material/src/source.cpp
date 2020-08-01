#include <monte-toad/core/any.hpp>
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

#include <vector>

namespace {

struct MaterialComponent {
  float probability = 0.0f;

  size_t pluginIdx = -1lu;
  mt::core::Any userdata;
};

struct Material {
  ::MaterialComponent emitter;
  std::vector<MaterialComponent> reflective, refractive;
  float indexOfRefraction;
};

} // -- anon namespace

extern "C" {

char const * PluginLabel() { return "base material"; }
mt::PluginType PluginType() { return mt::PluginType::Material; }

void Allocate(mt::core::Any & userdata) {
  if (userdata.data == nullptr) { free(userdata.data); }
  userdata.data = ::calloc(1, sizeof(::Material));
  auto & m = *reinterpret_cast<::Material*>(userdata.data);
  m = {};
  m.reflective.resize(0);
  m.refractive.resize(0);
}

bool IsEmitter(
  mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & /*plugin*/
) {
  auto const & material =
    *reinterpret_cast<::Material*>(
      scene.meshes[surface.material].material.data
    );

  return material.emitter.pluginIdx != -1lu;
}

mt::core::BsdfSampleInfo Sample(
  mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
) {
  auto const & material =
    *reinterpret_cast<::Material*>(
      scene.meshes[surface.material].material.data
    );

  mt::core::BsdfSampleInfo sampleInfo;

  bool reflection = true;
  // if IOR is <100, the material has a probability to refract
  if (material.indexOfRefraction < 100.0f)
  {
    auto const cosTheta = glm::dot(-surface.incomingAngle, surface.normal);
    glm::vec3 normal = surface.normal;
    float eta = material.indexOfRefraction;

    // flip normal if surface is incorrect for refraction
    if (glm::dot(-surface.incomingAngle, surface.normal) < 0.0f) {
      normal = -surface.normal;
      // eta = 1.0f/eta;
    }

    float const f0 = glm::sqr((1.0f - eta) / (1.0f + eta));

    float const fresnel = f0 + (1.0f - f0)*glm::pow(1.0f - cosTheta, 5.0f);

    reflection =
        fresnel <= 0.0001f
     || fresnel > plugin.random.SampleUniform1()
    ;

    // total internal reflection
    /* if (1.0f - eta*eta*(1.0f - cosTheta*cosTheta) < 0.0f) { */
    /*   reflection = true; */
    /* } */
  }

  if (!reflection) {
    // -- refraction
    if (material.refractive.size() == 0) { return mt::core::BsdfSampleInfo{}; }

    // choose probability, don't sample a uniform if there's only one brdf
    float const probability =
      material.refractive.size() <= 1 ? 0.0f : plugin.random.SampleUniform1()
    ;

    // locate the corresponding material and calculate results
    float probabilityIt = 0.0f;
    for (size_t i = 0; i < material.refractive.size(); ++i) {
      probabilityIt += material.refractive[i].probability;

      auto & bsdf = material.refractive[i];

      // probability found now just return the plugin's brdf sample
      if (probabilityIt >= probability) {
        return
          plugin.bsdfs[bsdf.pluginIdx].BsdfSample(
              bsdf.userdata, material.indexOfRefraction, plugin.random, surface
          );
      }
    }
  }

  // -- reflection
  if (material.reflective.size() == 0) { return mt::core::BsdfSampleInfo{}; }

  // choose probability, don't sample a uniform if there's only one brdf
  float const probability =
    material.reflective.size() <= 1 ? 0.0f : plugin.random.SampleUniform1()
  ;

  // locate the corresponding material and calculate results
  float probabilityIt = 0.0f;
  for (size_t i = 0; i < material.reflective.size(); ++i) {
    probabilityIt += material.reflective[i].probability;

    auto & bsdf = material.reflective[i];

    // probability found now just return the plugin's brdf sample
    if (probabilityIt >= probability) {
      return
        plugin.bsdfs[bsdf.pluginIdx].BsdfSample(
            bsdf.userdata, material.indexOfRefraction, plugin.random, surface
        );
    }
  }

  spdlog::critical(
    "material probability picker failed, idx {}", surface.material
  );

  return mt::core::BsdfSampleInfo{};
}

float Pdf(
  mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
, glm::vec3 const & wo
, bool const reflection
, size_t const componentIdx
) {
  auto const & material =
    *reinterpret_cast<::Material*>(
      scene.meshes[surface.material].material.data
    );
  auto & bsdf =
      reflection
    ? material.reflective[componentIdx]
    : material.refractive[componentIdx]
  ;

  return
    bsdf.probability
  * plugin.bsdfs[bsdf.pluginIdx].BsdfPdf(
      bsdf.userdata, material.indexOfRefraction, surface, wo
    )
  ;
}

glm::vec3 EmitterFs(
  mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
) {
  auto const & material =
    *reinterpret_cast<::Material*>(
      scene.meshes[surface.material].material.data
    );
  auto & bsdf = material.emitter;
  return
    plugin
      .bsdfs[bsdf.pluginIdx]
      .BsdfFs(
        bsdf.userdata, material.indexOfRefraction, surface, glm::vec3(0.0f)
      );
}

glm::vec3 Fs(
  mt::core::SurfaceInfo const & /*surface*/
, mt::core::Scene const & /*scene*/
, mt::PluginInfo const & /*plugin*/
, glm::vec3 const & /*wo*/
) {
  return glm::vec3(0.0f);
  /* auto & material = */
  /*     reflection */
  /*   ? scene.meshes[surface.material].material.reflection[componentIdx] */
  /*   : scene.meshes[surface.material].material.refraction[componentIdx] */
  /* ; */

  /* return plugin.material.BsdfFs(material.userdata, surface, wo); */
}

float IndirectPdf(
  mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
, glm::vec3 const & wo
) {
  // i think for this all PDFs have to be calculated and combined dependent on
  // their probability
  float pdf = 0.0f;

  bool reflection = glm::dot(surface.incomingAngle, surface.normal) > 0.0f;

  auto const & material =
    *reinterpret_cast<::Material*>(
      scene.meshes[surface.material].material.data
    );

  auto const & bsdfs = reflection ? material.reflective : material.refractive;

  for (auto const & bsdf : bsdfs) {
    pdf +=
      bsdf.probability
    * plugin.bsdfs[bsdf.pluginIdx]
        .BsdfPdf(bsdf.userdata, material.indexOfRefraction, surface, wo)
    ;
  }

  return pdf;
}

void UiUpdate(
  mt::core::Scene & scene
, mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  static size_t currentMtlIdx = -1lu;

  // -- if an image is clicked then update where it was clicked;
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

    render.lastIntegratorImageClicked = -1lu;
  }

  // reset in case something weird happens and idx is oob
  if (currentMtlIdx != -1lu && currentMtlIdx >= scene.meshes.size())
  { currentMtlIdx = 0; }

  // -- material editor
  if (!ImGui::Begin("Material editor")) {}

  if (currentMtlIdx == -1lu) {
    ImGui::End();
    return;
  }

  ImGui::Text("selected material idx %lu\n", currentMtlIdx);

  ImGui::Separator();

  auto & material =
    *reinterpret_cast<::Material*>(scene.meshes[currentMtlIdx].material.data);

  if (
    ImGui::SliderFloat(
      "index of refraction", &material.indexOfRefraction, 0.0f, 10.0f
    )
  ) {
    render.ClearImageBuffers();
  }

  ImGui::Separator();
  ImGui::Separator();
  ImGui::Text("-- reflective --");

  for (size_t bsdfIdx = 0; bsdfIdx < material.reflective.size(); ++ bsdfIdx) {
    auto & bsdf = material.reflective[bsdfIdx];
    auto & materialPlugin = plugin.bsdfs[bsdf.pluginIdx];
    ImGui::Separator();
    ImGui::PushID(fmt::format("{}", bsdfIdx).c_str());
    ImGui::Text("%s", materialPlugin.PluginLabel());
    if (ImGui::SliderFloat("%", &bsdf.probability, 0.0f, 1.0f)) {
      render.ClearImageBuffers();

      // normalize bsdf probabilities
      float total = 0.0f;
      for (auto const & tBsdf : material.reflective) {
        total += tBsdf.probability;
      }
      for (auto & tBsdf : material.reflective) {
        tBsdf.probability /= total;
      }
    }

    if (ImGui::Button("delete")) {
      material.reflective.erase(material.reflective.begin() + bsdfIdx);
      -- bsdfIdx;
    }

    if (
        materialPlugin.UiUpdate
     && ImGui::TreeNode(fmt::format("==================##{}", bsdfIdx).c_str())
    ) {
      materialPlugin.UiUpdate(bsdf.userdata, render, scene);
      ImGui::TreePop();
    }
  }

  ImGui::Separator();

  if (ImGui::BeginCombo("##brdf", "add brdf")) {
    ImGui::Selectable("cancel", true);
    for (size_t i = 0; i < plugin.bsdfs.size(); ++ i) {
      if (!plugin.bsdfs[i].IsReflective()) { continue; }
      if (ImGui::Selectable(plugin.bsdfs[i].PluginLabel())) {
        ::MaterialComponent component;
        component.probability = 1.0f;
        component.pluginIdx = i;
        plugin.bsdfs[i].Allocate(component.userdata);
        material.reflective.emplace_back(std::move(component));

        render.ClearImageBuffers();
      }
    }

    ImGui::EndCombo();
  }

  ImGui::Separator();
  ImGui::Separator();
  ImGui::Text("-- refractive --");
  for (size_t bsdfIdx = 0; bsdfIdx < material.refractive.size(); ++ bsdfIdx) {
    auto & bsdf = material.refractive[bsdfIdx];
    auto & materialPlugin = plugin.bsdfs[bsdf.pluginIdx];
    ImGui::Separator();
    ImGui::PushID(fmt::format("{}", bsdfIdx).c_str());
    ImGui::Text("%s", materialPlugin.PluginLabel());
    if (ImGui::SliderFloat("%", &bsdf.probability, 0.0f, 1.0f)) {
      render.ClearImageBuffers();

      // normalize bsdf probabilities
      float total = 0.0f;
      for (auto const & tBsdf : material.refractive) {
        total += tBsdf.probability;
      }
      for (auto & tBsdf : material.refractive) {
        tBsdf.probability /= total;
      }
    }

    if (ImGui::Button("delete")) {
      material.refractive.erase(material.refractive.begin() + bsdfIdx);
      -- bsdfIdx;
    }

    if (
        materialPlugin.UiUpdate
     && ImGui::TreeNode(fmt::format("==================##{}", bsdfIdx).c_str())
    ) {
      materialPlugin.UiUpdate(bsdf.userdata, render, scene);
      ImGui::TreePop();
    }
  }

  ImGui::Separator();

  if (ImGui::BeginCombo("##btdf", "add btdf")) {
    ImGui::Selectable("cancel", true);
    for (size_t i = 0; i < plugin.bsdfs.size(); ++ i) {
      if (!plugin.bsdfs[i].IsRefractive()) { continue; }
      if (ImGui::Selectable(plugin.bsdfs[i].PluginLabel())) {
        ::MaterialComponent component;
        component.probability = 1.0f;
        component.pluginIdx = i;
        plugin.bsdfs[i].Allocate(component.userdata);
        material.refractive.emplace_back(std::move(component));

        render.ClearImageBuffers();
      }
    }

    ImGui::EndCombo();
  }

  ImGui::Separator();
  ImGui::Separator();

  ImGui::Text("-- emitter --");

  if (ImGui::BeginCombo("##emitter", "add emitter")) {
    ImGui::Selectable("cancel", true);
    for (size_t i = 0; i < plugin.bsdfs.size(); ++ i) {
      if (ImGui::Selectable(plugin.bsdfs[i].PluginLabel())) {
        ::MaterialComponent component;
        component.probability = 1.0f;
        component.pluginIdx = i;
        plugin.bsdfs[i].Allocate(component.userdata);
        material.emitter = std::move(component);

        render.ClearImageBuffers();
      }
    }

    ImGui::EndCombo();
  }

  if (ImGui::Button("delete")) {
    material.emitter.pluginIdx = -1lu;
  }

  if (material.emitter.pluginIdx != -1lu) {
    auto & materialPlugin = plugin.bsdfs[material.emitter.pluginIdx];
    materialPlugin.UiUpdate(material.emitter.userdata, render, scene);
  }

  ImGui::End();
}

} // -- extern c
