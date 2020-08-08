#include <monte-toad/core/any.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/math.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/spectrum.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <monte-toad/core/texture.hpp>
#include <monte-toad/core/triangle.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

#include <vector>

namespace {

struct MaterialComponent {
  float probability = 0.0f;

  size_t pluginIdx = -1ul;
  mt::core::Any userdata;
};

struct Material {
  ::MaterialComponent emitter;
  std::vector<MaterialComponent> diffuse, specular, refractive;
  mt::core::TextureOption<float>
    indexOfRefraction { "index of refraction (IOR)", 1.0f, 5.0f, 1.0f }
  , fresnelMinimalReflection {
      "fresnel minimal reflection (F0)", 0.0f, 1.0f, 0.0f, 2.2f
    }
  ;
};

// TODO move to core or something
float FresnelReflectAmount(
  float const iorStart
, float const iorEnd
, float const f0, float const f90
, glm::vec3 const & normal
, glm::vec3 const & wi
) {
  // schlick approx but correct iorStart/iorEnd and check for TIR
  float r0 = (iorStart-iorEnd) / (iorStart+iorEnd);
  r0 *= r0;
  float cosTheta = dot(normal, wi);
  if (iorStart > iorEnd) {
    float const n = iorStart/iorEnd;
    float sinT2 = n*n*(1.0f - cosTheta*cosTheta);
    // total internal reflection
    if (sinT2 > 1.0f) { return f90; }
    cosTheta = glm::sqrt(1.0f - sinT2);
  }
  float const x = 1.0f - cosTheta;

  // adjust reflection multiplier
  return glm::mix(f0, f90, r0 + (1.0f-r0)*x*x*x*x*x);
}


mt::core::BsdfSampleInfo SampleMaterial(
  std::vector<MaterialComponent> const & component
, float const indexOfRefraction
, mt::core::SurfaceInfo const & surface
, mt::PluginInfo const & plugin
) {
  // choose probability, don't sample a uniform if there's only one brdf
  float const probability =
    component.size() <= 1 ? 0.0f : plugin.random.SampleUniform1()
  ;

  // locate the corresponding material and calculate results
  float probabilityIt = 0.0f;
  for (size_t i = 0; i < component.size(); ++i) {
    probabilityIt += component[i].probability;

    auto & bsdf = component[i];

    // probability found now just return the plugin's brdf sample
    if (probabilityIt >= probability) {
      return
        plugin.bsdfs[bsdf.pluginIdx].BsdfSample(
            bsdf.userdata, indexOfRefraction, plugin.random, surface
        );
    }
  }

  return mt::core::BsdfSampleInfo{};
}

void UiMaterialComponent(
  std::vector<MaterialComponent> & components
, mt::BsdfTypeHint const bsdfType
, std::string const & guiLabel
, mt::core::Scene & scene
, mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  ImGui::Text("%s", guiLabel.c_str());

  for (size_t bsdfIdx = 0; bsdfIdx < components.size(); ++ bsdfIdx) {
    auto & bsdf = components[bsdfIdx];
    auto & materialPlugin = plugin.bsdfs[bsdf.pluginIdx];
    ImGui::Separator();
    ImGui::PushID(fmt::format("{}", bsdfIdx).c_str());
    ImGui::Text("%s", materialPlugin.PluginLabel());
    if (ImGui::SliderFloat("%", &bsdf.probability, 0.0f, 1.0f)) {
      render.ClearImageBuffers();

      // normalize bsdf probabilities
      float total = 0.0f;
      for (auto const & tBsdf : components) {
        total += tBsdf.probability;
      }
      for (auto & tBsdf : components) {
        tBsdf.probability /= total;
      }
    }

    if (ImGui::Button("delete")) {
      components.erase(components.begin() + bsdfIdx);
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

  std::string const
    hiddenLabel = "##" + guiLabel
  , addLabel    = "add " + guiLabel
  ;

  if (ImGui::BeginCombo(hiddenLabel.c_str(), addLabel.c_str())) {
    ImGui::Selectable("cancel", true);
    for (size_t i = 0; i < plugin.bsdfs.size(); ++ i) {
      if (plugin.bsdfs[i].BsdfType() != bsdfType) { continue; }
      if (ImGui::Selectable(plugin.bsdfs[i].PluginLabel())) {
        ::MaterialComponent component;
        component.probability = 1.0f;
        component.pluginIdx = i;
        plugin.bsdfs[i].Allocate(component.userdata);
        components.emplace_back(std::move(component));

        render.ClearImageBuffers();
      }
    }

    ImGui::EndCombo();
  }
}

} // -- anon namespace

extern "C" {

char const * PluginLabel() { return "base material"; }
mt::PluginType PluginType() { return mt::PluginType::Material; }

void Allocate(mt::core::Any & userdata) {
  if (userdata.data == nullptr)
    { delete reinterpret_cast<::Material*>(userdata.data); }
  userdata.data = new ::Material{};
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
  float const ior = material.indexOfRefraction.Get(surface.uvcoord);

  if (
      material.specular.size() == 0ul
   && material.refractive.size() == 0ul
   && material.diffuse.size() == 0ul
  ) { return mt::core::BsdfSampleInfo{}; }

  mt::BsdfTypeHint sampleType = mt::BsdfTypeHint::Transmittive;
  float const fresnelMinimalReflection =
    material.fresnelMinimalReflection.Get(surface.uvcoord);
  float specularChance = fresnelMinimalReflection;
  float transmissionChance = 1.0f - fresnelMinimalReflection;

  if (specularChance > 0.0f) {
    specularChance =
      ::FresnelReflectAmount(
        surface.exitting ? ior : 1.0f
      , surface.exitting ? 1.0f : ior
      , fresnelMinimalReflection, 1.0f
      , surface.normal, -surface.incomingAngle
      );
  }

  if (transmissionChance > 0.0f) {
    transmissionChance = 1.0f - specularChance;
  }

  if (material.specular.size() == 0ul) { specularChance = 0.0f; }
  if (material.refractive.size() == 0ul) { transmissionChance = 0.0f; }

  float const fresnelProbability = plugin.random.SampleUniform1();
  if (specularChance > 0.0f && specularChance > fresnelProbability)
    { sampleType = mt::BsdfTypeHint::Specular; }
  else if (
      transmissionChance > 0.0f
   && transmissionChance + specularChance > fresnelProbability
  )
    { sampleType = mt::BsdfTypeHint::Transmittive; }
  else
    { sampleType = mt::BsdfTypeHint::Diffuse; }

  switch (sampleType) {
    default: break;
    case mt::BsdfTypeHint::Specular:
      return ::SampleMaterial(material.specular, ior, surface, plugin);
    case mt::BsdfTypeHint::Diffuse:
      return ::SampleMaterial(material.diffuse, ior, surface, plugin);
    case mt::BsdfTypeHint::Transmittive:
      return ::SampleMaterial(material.refractive, ior, surface, plugin);
  }

  assert(0);
  return mt::core::BsdfSampleInfo{};
}

float Pdf(
  mt::core::SurfaceInfo const & /*surface*/
, mt::core::Scene const & /*scene*/
, mt::PluginInfo const & /*plugin*/
, glm::vec3 const & /*wo*/
, bool const /*reflection*/
, size_t const /*componentIdx*/
) {
  /* auto const & material = */
  /*   *reinterpret_cast<::Material*>( */
  /*     scene.meshes[surface.material].material.data */
  /*   ); */
  // TODO
  return 0.0f;
  /* auto & bsdf = */
  /*     reflection */
  /*   ? material.reflective[componentIdx] */
  /*   : material.refractive[componentIdx] */
  /* ; */

  /* return */
  /*   bsdf.probability */
  /* * plugin.bsdfs[bsdf.pluginIdx].BsdfPdf( */
  /*     bsdf.userdata, material.indexOfRefraction, surface, wo */
  /*   ) */
  /* ; */
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
        bsdf.userdata, material.indexOfRefraction.Get(surface.uvcoord)
      , surface, glm::vec3(0.0f)
      );
}

glm::vec3 BsdfFs(
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

glm::vec3 AlbedoApproximation(
  mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
) {
  auto const & material =
    *reinterpret_cast<::Material*>(
      scene.meshes[surface.material].material.data
    );
  auto fresnelMin = material.fresnelMinimalReflection.Get(surface.uvcoord);
  auto ior = material.indexOfRefraction.Get(surface.uvcoord);
  bool const hasSpecular =
      material.specular.size() > 0ul
   && fresnelMin >= 0.01f
  ;

  bool const hasDiffuse = material.diffuse.size();

  bool const hasRefractive =
      material.refractive.size() > 0ul
   && material.fresnelMinimalReflection.Get(surface.uvcoord) <= 0.99f
  ;

  // TODO if specular or refractive then want to probably take a first-attempt
  // guess via raycast

  // get diffuse
  glm::vec3 diff = glm::vec3(0.0f);
  for (auto & bsdf : material.diffuse) {
    diff +=
        bsdf.probability
      * plugin
          .bsdfs[bsdf.pluginIdx]
          .AlbedoApproximation(bsdf.userdata, ior, surface);
  }

  // get refraction
  glm::vec3 refr = glm::vec3(0.0f);
  for (auto & bsdf : material.refractive) {
    refr +=
        bsdf.probability
      * plugin
          .bsdfs[bsdf.pluginIdx]
          .AlbedoApproximation(bsdf.userdata, ior, surface);
  }

  // get specular
  glm::vec3 spec = glm::vec3(0.0f);
  for (auto & bsdf : material.specular) {
    spec +=
        bsdf.probability
      * plugin
          .bsdfs[bsdf.pluginIdx]
          .AlbedoApproximation(bsdf.userdata, ior, surface);
  }

  if (!hasRefractive && !hasSpecular) { return diff; }
  if (!hasDiffuse && hasSpecular) { return spec; }
  if (!hasDiffuse && hasRefractive) { return refr; }

  if (hasSpecular && !hasRefractive)
    { return glm::mix(diff, spec, fresnelMin); }

  if (hasSpecular && hasRefractive)
    { return glm::mix(refr, spec, fresnelMin); }

  return glm::mix(refr, diff, fresnelMin);
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

  auto const & bsdfs = reflection ? material.diffuse : material.refractive;

  for (auto const & bsdf : bsdfs) {
    pdf +=
      bsdf.probability
    * plugin.bsdfs[bsdf.pluginIdx]
        .BsdfPdf(
          bsdf.userdata, material.indexOfRefraction.Get(surface.uvcoord)
        , surface, wo
        )
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

    auto const camera =
      plugin.camera.Dispatch(
          plugin.random, render.camera, data.imageResolution, uv
          );

    auto const surface =
      mt::core::Raycast(scene, plugin, camera.origin, camera.direction, -1ul);

    currentMtlIdx =
      static_cast<size_t>(surface.Valid() ? surface.triangle.MeshIdx() : -1ul);

    render.lastIntegratorImageClicked = -1lu;
  }

  // reset in case something weird happens and idx is oob
  if (currentMtlIdx != -1lu && currentMtlIdx >= scene.meshes.size())
  { currentMtlIdx = 0; }

  // -- material editor
  ImGui::Begin("Material editor");

  if (currentMtlIdx == -1lu) {
    if (scene.meshes.size() > 0 && ImGui::Button("+"))
      { currentMtlIdx = 0lu; }
    ImGui::End();
    return;
  }

  if (ImGui::Button("-")) {
    currentMtlIdx =
      (currentMtlIdx - 1 + scene.meshes.size()) % scene.meshes.size();
  }

  ImGui::SameLine();
  if (ImGui::Button("+")) {
    currentMtlIdx = (currentMtlIdx + 1lu) % scene.meshes.size();
  }

  ImGui::SameLine();
  ImGui::Text(
    "selected material %lu / %lu", currentMtlIdx, scene.meshes.size()-1
  );

  ImGui::Separator();

  auto & material =
    *reinterpret_cast<::Material*>(scene.meshes[currentMtlIdx].material.data);

  if (material.indexOfRefraction.GuiApply(scene))
    { render.ClearImageBuffers(); }

  ImGui::Separator();

  if (material.fresnelMinimalReflection.GuiApply(scene))
    { render.ClearImageBuffers(); }

  ImGui::Separator();
  ImGui::Separator();

  ::UiMaterialComponent(
    material.diffuse, mt::BsdfTypeHint::Diffuse, "diffuse"
  , scene, render, plugin
  );

  ImGui::Separator();
  ImGui::Separator();

  ::UiMaterialComponent(
    material.specular, mt::BsdfTypeHint::Specular, "specular"
  , scene, render, plugin
  );

  ImGui::Separator();
  ImGui::Separator();

  ::UiMaterialComponent(
    material.refractive, mt::BsdfTypeHint::Transmittive, "transmittive"
  , scene, render, plugin
  );

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

  if (material.emitter.pluginIdx != -1lu) {
    if (ImGui::Button("delete")) {
      material.emitter.pluginIdx = -1lu;
      // TODO can't delete bc don't know type
    }
  }

  if (material.emitter.pluginIdx != -1lu) {
    auto & materialPlugin = plugin.bsdfs[material.emitter.pluginIdx];
    materialPlugin.UiUpdate(material.emitter.userdata, render, scene);
  }

  // -- sanity checks for material, would be better if uimaterialcomponent
  //    returned a value so this can happen on those events TODO
  if (
      material.diffuse.size() == 0
   && material.refractive.size() == 0
   && material.specular.size() > 0
  ) {
    material.fresnelMinimalReflection.userValue = 1.0f;
  }

  if (
      material.diffuse.size() == 0
   && material.specular.size() == 0
   && material.refractive.size() > 0
  ) {
    material.fresnelMinimalReflection.userValue = 0.0f;
  }

  ImGui::End();
}

} // -- extern c
