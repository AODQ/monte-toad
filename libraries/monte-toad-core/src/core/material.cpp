#include <monte-toad/core/material.hpp>

#include <monte-toad/core/log.hpp>
#include <monte-toad/core/math.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/spectrum.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <mt-plugin/plugin.hpp>

mt::core::BsdfSampleInfo mt::core::MaterialSample(
  mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
) {
  auto & material = scene.meshes[surface.material].material;

  mt::core::BsdfSampleInfo sampleInfo;

  bool reflection = true;
  // if IOR is <100, the material has a probability to refract
  if (material.indexOfRefraction < 100.0f)
  {
    auto const cosTheta = glm::dot(-surface.incomingAngle, surface.normal);
    glm::vec3 normal = surface.normal;
    float eta = material.indexOfRefraction;

    // flip normal if surface is incorrect for refraction
    if (glm::dot(surface.incomingAngle, surface.normal) > 0.0f) {
      normal = -surface.normal;
      eta = 1.0f/eta;
    }

    float const f0 = glm::sqr((1.0f - eta) / (1.0f + eta));

    float const fresnel = f0 + (1.0f - f0)*glm::pow(1.0f - cosTheta, 5.0f);

    reflection =
        fresnel <= 0.0001f
     || fresnel > plugin.random.SampleUniform1()
    ;

    // total internal reflection
    if (1.0f - eta*eta*(1.0f - cosTheta*cosTheta) < 0.0f) {
      reflection = true;
    }
  }

  if (!reflection) {
    // -- refraction
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

    // probability found now just return the plugin's brdf sample
    if (probabilityIt >= probability) {
      return
        plugin.materials[material.reflective[i].pluginIdx].BsdfSample(
          material.reflective[i].userdata, plugin.random, surface
        );
    }
  }

  spdlog::critical(
    "material probability picker failed, idx {}", surface.material
  );

  return mt::core::BsdfSampleInfo{};
}

float mt::core::MaterialPdf(
  mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
, glm::vec3 const & wo
, bool const reflection
, size_t const componentIdx
) {
  auto & material =
      reflection
    ? scene.meshes[surface.material].material.reflective[componentIdx]
    : scene.meshes[surface.material].material.refractive[componentIdx]
  ;

  return
    material.probability
  * plugin.materials[material.pluginIdx].BsdfPdf(material.userdata, surface, wo)
  ;
}

glm::vec3 mt::core::MaterialFs(
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

float mt::core::MaterialIndirectPdf(
  mt::core::SurfaceInfo const & surface
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
, glm::vec3 const & wo
) {
  // i think for this all PDFs have to be calculated and combined dependent on
  // their probability
  float pdf = 0.0f;

  bool reflection = glm::dot(surface.incomingAngle, surface.normal) > 0.0f;

  auto const & materials =
      reflection
    ? scene.meshes[surface.material].material.reflective
    : scene.meshes[surface.material].material.refractive
  ;

  for (auto const & mat : materials) {
    pdf +=
        mat.probability
      * plugin.materials[mat.pluginIdx].BsdfPdf(mat.userdata, surface, wo)
    ;
  }

  return pdf;
}
