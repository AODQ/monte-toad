// primary dispatcher

#include <monte-toad/core/camerainfo.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/debugutil/integratorpathunit.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>

namespace mt::core { struct Scene; }
namespace mt { struct PluginInfo; }
namespace mt { struct RenderInfo; }

namespace
{
std::vector<mt::debugutil::IntegratorPathUnit> storedPathRecorder;
mt::PixelInfo storedPixelInfo;
mt::core::CameraInfo storedCamera;

void RecordPath(mt::debugutil::IntegratorPathUnit unit) {
  storedPathRecorder.emplace_back(std::move(unit));
}

template <typename Fn>
void BresenhamLine(glm::ivec2 f0, glm::ivec2 f1, Fn && fn) {

  bool steep = false;
  if (glm::abs(f0.x-f1.x) < glm::abs(f0.y-f1.y)) {
    std::swap(f0.x, f0.y);
    std::swap(f1.x, f1.y);
    steep = true;
  }

  if (f0.x > f1.x)
    { std::swap(f0, f1); }

  int32_t
    dx = f1.x-f0.x
  , dy = f1.y-f0.y
  , derror = glm::abs(dy)*2
  , error = 0.0f
  , stepy = f1.y > f0.y ? +1.0f : -1.0f
  ;

  for (glm::ivec2 f = f0; f.x <= f1.x; ++ f.x) {
    if (steep) { fn(f.y, f.x); } else { fn(f.x, f.y); }
    error += derror;
    if (error > dx) {
      f.y += stepy;
      error -= dx*2;
    }
  }
}

void DrawPath(mt::PluginInfo const & plugin, mt::core::RenderInfo & render) {
  auto & depthIntegratorData =
    render.integratorData[
      render.integratorIndices[Idx(mt::IntegratorTypeHint::Depth)]
    ];

  if (!plugin.camera.WorldCoordToUv)
    { spdlog::error("Need camera plugin `WorldCoordToUv` implemented"); }

  for (size_t i = 0; i < storedPathRecorder.size()-1; ++ i) {
    glm::vec2 const
      uvCoordStart =
        plugin.camera.WorldCoordToUv(
          storedCamera,
          storedPathRecorder[i].surface.origin
        )
    , uvCoordEnd =
        plugin.camera.WorldCoordToUv(
          storedCamera,
          storedPathRecorder[i+1].surface.origin
        )
    ;

    glm::uvec2 const depthResolution =
      glm::uvec2(depthIntegratorData.imageResolution);

    glm::uvec2
      uCoordStart = glm::uvec2(uvCoordStart * glm::vec2(depthResolution))
    , uCoordEnd   = glm::uvec2(uvCoordEnd   * glm::vec2(depthResolution))
    ;

    // -- bresenham line algorithm
    BresenhamLine(
      glm::ivec2(uCoordStart)
    , glm::ivec2(uCoordEnd)
    , [&](int32_t x, int32_t y) {
        size_t idx = y*depthResolution.x + x;
        if (idx < depthIntegratorData.mappedImageTransitionBuffer.size()) {
          depthIntegratorData.mappedImageTransitionBuffer[idx] =
            glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        }
      }
    );
  }

  mt::core::DispatchImageCopy(
    depthIntegratorData
  , 0, depthIntegratorData.imageResolution.x
  , 0, depthIntegratorData.imageResolution.y
  );
}
}

extern "C" {

char const * PluginLabel() { return "primary dispatcher"; }
mt::PluginType PluginType() { return mt::PluginType::Dispatcher; }

void UiUpdate(
  mt::core::Scene & /*scene*/
, mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  ImGui::Begin("dispatchers");

  size_t const
    primaryIntegratorIdx =
      render.integratorIndices[Idx(mt::IntegratorTypeHint::Primary)]
  , depthIntegratorIdx =
      render.integratorIndices[Idx(mt::IntegratorTypeHint::Depth)]
  ;

  if (primaryIntegratorIdx != -1lu && depthIntegratorIdx != -1lu) {
    if (ImGui::Button("Record path")) {
      ::storedPathRecorder.clear();

      ::storedCamera = render.camera;

      /* ::storedPixelInfo = */
      /*   plugin */
      /*     .integrators[primaryIntegratorIdx] */
      /*     .Dispatch( */
      /*       glm::vec2(0.0f), scene, render.camera, plugin, primaryIntegratorData */
      /*     , ::RecordPath */
      /*     ); */
      (void)RecordPath;
      ::storedPathRecorder = {{
        glm::vec3(1.0f), glm::vec3(0.0f), mt::TransportMode::Radiance,
        0, mt::core::SurfaceInfo::Construct(glm::vec3(-0.3f, 1.4f, 3.0f), glm::vec3(0.0f))
      },{ glm::vec3(1.0f), glm::vec3(0.0f), mt::TransportMode::Radiance,
        0, mt::core::SurfaceInfo::Construct(glm::vec3(-0.36f, 0.548f, -0.033f), glm::vec3(0.0f))
      },{ glm::vec3(1.0f), glm::vec3(0.0f), mt::TransportMode::Radiance,
        0, mt::core::SurfaceInfo::Construct(glm::vec3(0.976f, 1.564f, 0.923f), glm::vec3(0.0f))
      }};
    }

    if (::storedPathRecorder.size() > 0) {
      ImGui::SameLine();
      static bool displayPath = false;
      ImGui::Checkbox("Display path", &displayPath);
      if (displayPath) { ::DrawPath(plugin, render); }
    }
  }

  ImGui::Text(
    "%s"
  , fmt::format(
      "Recorded path valid ({}) | value ({})"
    , ::storedPixelInfo.valid ? "yes" : "no"
    , ::storedPixelInfo.color
    ).c_str()
  );

  for (size_t i = 0; i < ::storedPathRecorder.size(); ++ i) {
    auto const & unit = ::storedPathRecorder[i];
    ImGui::Separator();
    ImGui::Text(
      "%s"
    , fmt::format(
        "origin: {}"
      , unit.surface.origin
      ).c_str()
      /* fmt::format( */
      /*   "it {}\n\t radiance {} accumulatedIrradiance {}" */
      /*   "\n\ttransport mode {}" */
      /*   "\n\twi {}\n\tnormal {}\n\torigin {}\n\tdistance {} exitting {}" */
      /* , unit.it, unit.radiance, unit.accumulatedIrradiance */
      /* , unit.transportMode == mt::TransportMode::Importance */
      /*   ? "Importance" : "Radiance" */
      /* , unit.surface.incomingAngle */
      /* , unit.surface.normal */
      /* , unit.surface.origin */
      /* , unit.surface.distance */
      /* , unit.surface.exitting */
      /* ).c_str() */
    );
  }

  ImGui::End();
}

void DispatchBlockRegion(
  mt::core::Scene const & scene
, mt::core::RenderInfo & render
, mt::PluginInfo const & plugin

, size_t integratorIdx
, size_t const minX, size_t const minY
, size_t const maxX, size_t const maxY
, size_t strideX, size_t strideY
, size_t internalIterator
) {
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
  for (size_t y = minY; y < maxY; y += strideY)
  for (size_t it = 0; it < internalIterator; ++ it) {
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
        .Dispatch(uv, scene, render.camera, plugin, integratorData, nullptr);

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
  }
}

} // extern C
