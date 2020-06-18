#include <monte-toad/engine.hpp>

#include <monte-toad/imagebuffer.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>

#include <mt-plugin/plugin.hpp>

#include <omp.h>

void mt::DispatchEngineBlockRegion(
  mt::Scene const & scene
, mt::RenderInfo & render
, mt::PluginInfo const & plugin
, size_t integratorIdx
, size_t const minX, size_t const minY
, size_t const maxX, size_t const maxY
, size_t strideX, size_t strideY
) {
  [[maybe_unused]] size_t progress = 0u;

  [[maybe_unused]] auto const masterTid = omp_get_thread_num();
  [[maybe_unused]] size_t mainThreadUpdateIt;

  auto & integratorData = render.integratorData[integratorIdx];

  auto const & resolution = integratorData.imageResolution;
  auto const resolutionAspectRatio =
    resolution.y / static_cast<float>(resolution.x);

  if (minX > resolution.x || maxX > resolution.x) {
    spdlog::critical(
      "minX ({}) and maxX({}) not in resolution bounds ({})",
      minX, maxX, resolution.x
    );
    return;
  }

  if (minY > resolution.y || maxY > resolution.y) {
    spdlog::critical(
      "minY ({}) and maxY({}) not in resolution bounds ({})",
      minY, maxY, resolution.y
    );
    return;
  }

  #pragma omp parallel for
  for (size_t x = minX; x < maxX; x += strideX)
  for (size_t y = minY; y < maxY; y += strideY) {
    auto & pixelCount = integratorData.pixelCountBuffer[y*resolution.x + x];
    auto & pixel =
      integratorData.mappedImageTransitionBuffer[y*resolution.x + x];

    if (pixelCount >= integratorData.samplesPerPixel) { continue; }

    glm::vec2 uv = glm::vec2(x, y) / glm::vec2(resolution.x, resolution.y);
    uv = (uv - glm::vec2(0.5f)) * 2.0f;
    uv.y *= resolutionAspectRatio;

    auto pixelResults =
      plugin
        .integrators[integratorIdx]
        .Dispatch(uv, scene, render, plugin, integratorData);

    auto & pixelCount = integratorData.pixelCountBuffer[y*resolution.x + x];
    auto & pixel =
      integratorData.mappedImageTransitionBuffer[y*resolution.x + x];

    if (pixelResults.valid) {
      pixel =
        glm::vec4(
          glm::mix(
            pixelResults.color,
            glm::vec3(pixel),
            pixelCount / static_cast<float>(pixelCount + 1)
          ),
          1.0f
        );
      ++ pixelCount;
    }

    /* if (headless */
    /*  && omp_get_thread_num() == masterTid && (++mainThreadUpdateIt)%5 == 0 */
    /* ) { */
    /*   PrintProgress( */
    /*     progress/static_cast<float>(resolutionDim) */
    /*   ); */
    /* } */
  }
}
