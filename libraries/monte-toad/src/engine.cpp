#include <monte-toad/engine.hpp>

#include <monte-toad/imagebuffer.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/scene.hpp>

#include <mt-plugin/plugin.hpp>

#include <omp.h>

void mt::DispatchEngineBlockRegion(
    mt::Scene const & scene
  , span<glm::vec4> imageBuffer
  , mt::RenderInfo & renderInfo
  , mt::PluginInfo & pluginInfo
  , size_t const min, const size_t max
  , bool headless
) {

  // make sure plugin info is valid
  if (pluginInfo.integrator.Dispatch == nullptr) { return; }

  size_t progress = 0u;

  auto const masterTid = omp_get_thread_num();
  size_t mainThreadUpdateIt;

  auto const & resolution =
    glm::uvec2(renderInfo.imageResolution[0], renderInfo.imageResolution[1]);
  auto const resolutionDim = resolution.x * resolution.y;
  auto const resolutionAspectRatio =
    resolution[1] / static_cast<float>(resolution.x);

  #pragma omp parallel for
  for (size_t i = min; i < max; ++ i) {
    size_t x = i%resolution.x, y = i/resolution.x;
    glm::vec2 uv = glm::vec2(x, y) / glm::vec2(resolution.x, resolution.y);
    uv = (uv - glm::vec2(0.5f)) * 2.0f;
    uv.y *= resolutionAspectRatio;

    imageBuffer[y*resolution.x + x] =
      glm::vec4(
        pluginInfo
          .integrator
          .Dispatch(uv, resolution, scene, renderInfo, pluginInfo),
         1.0f
      );

    if (headless
     && omp_get_thread_num() == masterTid && (++mainThreadUpdateIt)%5 == 0
    ) {
      PrintProgress(
        progress/static_cast<float>(resolutionDim)
      );
    }
  }

  if (headless) { PrintProgress(1.0f); printf("\n"); }
}
