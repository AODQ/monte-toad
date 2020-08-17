// primary dispatcher

#include <monte-toad/core/camerainfo.hpp>
#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/debugutil/integratorpathunit.hpp>
#include <mt-plugin/plugin.hpp>

#include <imgui/imgui.hpp>
#include <omp.h>

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
            glm::vec3(1.0f, 0.0f, 0.0f);
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

void DispatchBlockRegion(
  mt::core::Scene const & scene
, mt::core::RenderInfo & render
, mt::PluginInfo const & plugin

, size_t integratorIdx
, size_t const minX, size_t const minY
, size_t const maxX, size_t const maxY
, size_t strideX, size_t strideY
, size_t internalIterator
, bool checkSamplesPerPixel = true
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

  #pragma omp parallel for collapse(2)
  for (size_t x = minX; x < maxX; x += strideX)
  for (size_t y = minY; y < maxY; y += strideY)
  for (size_t it = 0; it < internalIterator; ++ it) {
    auto & pixelCount = integratorData.pixelCountBuffer[y*resolution.x + x];
    auto & pixel =
      integratorData.mappedImageTransitionBuffer[y*resolution.x + x];

    if (checkSamplesPerPixel && pixelCount >= integratorData.samplesPerPixel)
      { continue; }

    glm::vec2 uv = glm::vec2(x, y) / glm::vec2(resolution.x, resolution.y);
    uv.x = 1.0f - uv.x; // flip X axis for image
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

void BlockCalculateRange(
  mt::core::IntegratorData & self
, glm::u16vec2 & minRange, glm::u16vec2 & maxRange
) {
  // amount of blocks that take up an image
  auto blockResolution =
    glm::u16vec2(
      glm::ceil(
        glm::vec2(self.imageResolution)
      / glm::vec2(self.blockIteratorStride)
      )
    );

  // get current block position in image then multiply it by stride, which
  // converts it to a specific pixel
  minRange =
    glm::u16vec2(
      self.blockIterator % blockResolution.x
    , self.blockIterator / blockResolution.x
    )
  * glm::u16vec2(self.blockIteratorStride);

  // clamp it using min (ei weird resolution) then add stride to max range
  minRange = glm::min(minRange, self.imageResolution);
  maxRange =
    glm::min(
      self.imageResolution
    , minRange + glm::u16vec2(self.blockIteratorStride)
    );
}

void BlockCollectFinishedPixels(
  mt::core::IntegratorData & self
, mt::PluginInfoIntegrator const & plugin
) {
  if (plugin.RealTime()) {
    self.renderingFinished = true;
    return;
  }

  glm::u16vec2 minRange, maxRange;
  ::BlockCalculateRange(self, minRange, maxRange);

  // find the next multiple of N (N-1 => N, N => N, N+1 => N*2) for maxRange
  // this is so that an entire block can be taken into account if the image
  // resolution isn't divisible by the stride; this could be optimized into a
  // single mathematical expression, instead of the for-loop, in the future if
  // wanted
  auto const & stride = self.blockIteratorStride;
  maxRange.x = maxRange.x + ((stride-maxRange.x) % stride);
  maxRange.y = maxRange.y + ((stride-maxRange.y) % stride);

  size_t finishedPixels = 0;
  for (size_t x = minRange.x; x < maxRange.x; ++ x)
  for (size_t y = minRange.y; y < maxRange.y; ++ y) {
    if (y*self.imageResolution.x + x >= self.pixelCountBuffer.size()) {
      ++ finishedPixels;
      continue;
    }

    finishedPixels +=
      static_cast<size_t>(
        self.pixelCountBuffer[y*self.imageResolution.x + x]
      >= self.samplesPerPixel
      );
  }

  self.blockPixelsFinished[self.blockIterator] = finishedPixels;
}

void BlockIterate(
  mt::core::IntegratorData & self
, glm::u16vec2 & minRange, glm::u16vec2 & maxRange
) {
  size_t const
    blockPixelCount = self.blockIteratorStride*self.blockIteratorStride;

  ::BlockCalculateRange(self, minRange, maxRange);

  // amount of blocks that take up an image
  auto const blockResolution =
    glm::u16vec2(
      glm::ceil(
        glm::vec2(self.imageResolution)
      / glm::vec2(self.blockIteratorStride)
      )
  );

  auto center = blockResolution/glm::u16vec2(2ul);
  if (!(blockResolution.x % 2ul)) center.x -= 1;
  if (!(blockResolution.y % 2ul)) center.y -= 1;

  // -- perform iteration
  switch (self.previewDispatch ? self.previewDispatchType : self.dispatchType) {
    case mt::IntegratorDispatchType::StrideBlock: {
      self.blockIterator =
        (self.blockIterator + 1) % self.blockPixelsFinished.size();

      // -- skip blocks that are full
      size_t blockFull = 0ul;
      while (
        self.blockPixelsFinished[self.blockIterator] >= blockPixelCount
      ) {
        // if we have come full circle, then rendering has finished
        if (++ blockFull == self.blockPixelsFinished.size()) {
          self.renderingFinished = true;
          self.endTime = std::chrono::system_clock::now();
          return;
        }

        self.blockIterator =
          (self.blockIterator+1) % self.blockPixelsFinished.size();
      }
    } break;
    case mt::IntegratorDispatchType::FillBlockCw: {
      // -- get x/y coords to make iterating in spiral easier

      int32_t
        x = static_cast<int32_t>(self.blockIterator % blockResolution.x)
      , y = static_cast<int32_t>(self.blockIterator / blockResolution.x)
      ;

      // -- iterate to next block, keeping in mind that there are cases where
      //    spiral iterator has to wrap around OOB region in order to capture
      //    remaining blocks
      size_t blockFull = 0ul;
      while (
          (
            x < 0 || x >= blockResolution.x
         || y < 0 || y >= blockResolution.y
          )
        || self.blockPixelsFinished[self.blockIterator] >= blockPixelCount
      ) {

        // if we have come full circle, then rendering has finished, but only
        // increment blockFull if we're in the correct region
        if (x >= 0 && x < blockResolution.x && y >= 0 && y < blockResolution.y){
          if (++ blockFull == self.blockPixelsFinished.size()) {
            x = center.x; y = center.y;
            self.blockIterator = y*blockResolution.x + x;
            self.renderingFinished = true;
            self.endTime = std::chrono::system_clock::now();
            self.generatePreviewOutput = true;
            return;
          }
        }

        switch (self.fillBlockLeg) {
          case 0:
            if (++ x == static_cast<int32_t>(self.fillBlockLayer + center.x))
              ++ self.fillBlockLeg;
            break;
          case 1:
            if (++ y == static_cast<int32_t>(self.fillBlockLayer + center.y))
              ++ self.fillBlockLeg;
            break;
          case 2:
            if (-- x == static_cast<int32_t>(center.x - self.fillBlockLayer))
              ++ self.fillBlockLeg;
            break;
          case 3:
            if (-- y == static_cast<int32_t>(center.y - self.fillBlockLayer)) {
              ++ self.fillBlockLayer;
              self.fillBlockLeg = 0ul;
            }
          break;
        }

        // in this case the spiral has completed, & position needs to be reset
        // and loop broken out of
        if (x >= blockResolution.x-1 && y >= blockResolution.y) {
          x = center.x; y = center.y;
          self.blockIterator = y*blockResolution.x + x;
          self.fillBlockLeg = 0ul;
          self.fillBlockLayer = 1ul;
          break;
        }

        // convert x/y back to block iterator
        self.blockIterator = y*blockResolution.x + x;
      }
    } break;
  }

  // disable preview dispatch if preview has iterated through entire image
  if (self.blockIterator == 0ul && self.previewDispatch) {
    self.previewDispatch = false;
    auto x = center.x, y = center.y;
    self.blockIterator = y*blockResolution.x + x;
    self.generatePreviewOutput = true;
  }
}

void PrepareKernels(
  mt::core::RenderInfo & render
, mt::core::IntegratorData & data
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
) {
  // check if theere is a preview available to prepare
  if (!data.HasPreview() || !data.generatePreviewOutput) { return; }

  // collect integrators that need to be calculated
  std::vector<size_t> secondaryHintIdx;
  std::vector<size_t> secondaryIntegratorIdx;

  // go through integrators (excluding primary) and fill out secondary images
  // if they have not been set yet
  for (auto hintI = 0ul; hintI < Idx(mt::IntegratorTypeHint::Size); ++ hintI) {
    auto const integratorIdx = render.integratorIndices[hintI];

    // if this integrator is valid
    if (integratorIdx >= plugin.integrators.size()) { continue; }

    // if the secondary integrator has already been calculated
    if (data.secondaryIntegratorImagePtrs[integratorIdx].data()) { continue; }

    // TODO at some point it would be nice to link integrators to secondary
    //       integrators; this could be done automatically but then there would
    //       be a hidden cost/optimization that is non-obvious. To explicitly
    //       apply this optimization would be preferrable, not to mention I
    //       don't think it would be used very often anyways
    //  So for now calculations are done even redundantly, mostly because it
    //  would be awkward to pair an integrator index up to a specific
    //  integrator that shares camera information at this point when no actual
    //  integrator synchronization exists

    // store information if it needs to be calculated
    secondaryHintIdx.emplace_back(hintI);
    secondaryIntegratorIdx.emplace_back(integratorIdx);

    // allocate memory for secondary integrator
    data
      .secondaryIntegratorImages[hintI]
      .resize(data.mappedImageTransitionBuffer.size());

    data.secondaryIntegratorImagePtrs[hintI] =
      make_span(data.secondaryIntegratorImages[hintI]);
  }

  // check if any secondary integrator work needs to be computed
  if (secondaryHintIdx.size() == 0ul) { return; }
  assert(secondaryHintIdx.size() == secondaryIntegratorIdx.size());

  // basically do the same dispatch as elsewhere TODO this is copy/paste so
  // probably should be a function
  #pragma omp parallel for collapse(2)
  for (size_t x = 0; x < data.imageResolution.x; ++ x)
  for (size_t y = 0; y < data.imageResolution.y; ++ y) {

    glm::vec2 uv =
      glm::vec2(x, y)
    / glm::vec2(data.imageResolution.x, data.imageResolution.y)
    ;

    auto const resolutionAspectRatio =
      data.imageResolution.y / static_cast<float>(data.imageResolution.x);

    uv.x = 1.0f - uv.x; // flip X axis for image
    uv = (uv - glm::vec2(0.5f)) * 2.0f;
    uv.y *= resolutionAspectRatio;

    // TODO realtime probably should have hardcoded UV offsets to be
    //      consistent
    auto const eye =
      plugin.camera.Dispatch(
        plugin.random, render.camera
      , data.imageResolution
      , uv
      );

    // apply raycast to surface for secondary integrators, as they should all
    // share the same surface information
    auto const surface =
      mt::core::Raycast(scene, plugin, eye.origin, eye.direction, -1ul);

    for (size_t idx = 0ul; idx < secondaryHintIdx.size(); ++ idx) {
      auto const integratorIdx = secondaryIntegratorIdx[idx];
      auto const hintIdx       = secondaryHintIdx[idx];

      auto & pixel =
        data.secondaryIntegratorImages[hintIdx][y*data.imageResolution.x + x];

      auto pixelResults =
        plugin
          .integrators[integratorIdx]
          .DispatchRealtime(
            uv, surface, scene, plugin, render.integratorData[integratorIdx]
          );

      pixel = pixelResults.color;
    }
  }
}

// used to collect synced integrators that can share raycast results
std::vector<std::vector<size_t>> syncedIntegrators;

}

extern "C" {

char const * PluginLabel() { return "primary dispatcher"; }
mt::PluginType PluginType() { return mt::PluginType::Dispatcher; }

void DispatchRender(
  mt::core::RenderInfo & render
, mt::core::Scene const & scene
, mt::PluginInfo const & plugin
) {

  // -- collect synced integrators that can share raycast results
  ::syncedIntegrators.clear();
  ::syncedIntegrators.reserve(plugin.integrators.size());
  for (size_t idx = 0ul; idx < plugin.integrators.size(); ++ idx) {
    auto & self = render.integratorData[idx];

    // check if integrator has anything to be rendered
    if (self.renderingState == mt::RenderingState::Off) { continue; }
    if (self.renderingFinished) { continue; }

    // go through synced integrators to look for any lists to sync with
    for (auto & syncInt : ::syncedIntegrators) {
      auto & other = render.integratorData[syncInt[0]];

      if (other.imageResolution != self.imageResolution) { continue; }
      if (
          plugin.integrators[syncInt[0]].RealTime()
       != plugin.integrators[idx].RealTime()
      ) {
        continue;
      }

      // TODO in future should check that same camera is being used.

      // at this point they are similar enough to be rendered together
      syncInt.emplace_back(idx);
      goto POST_SYNC_LIST_APPEND;
    }

    // otherwise append it to the end as its own sync list
    ::syncedIntegrators.emplace_back(std::vector<size_t>{idx});

    POST_SYNC_LIST_APPEND:;
  }

  // iterate through all integrators and run either their low or high quality
  // dispatches
  for (auto const & syncIt : ::syncedIntegrators) {

    // if sync is realtime then it can be accelerated by sharing surface info
    if (plugin.integrators[syncIt[0]].RealTime()) {
      auto const resolution = render.integratorData[syncIt[0]].imageResolution;
      auto const resolutionAspectRatio =
        resolution.y / static_cast<float>(resolution.x);

      // -- apply update to integrator data metadata
      for (auto const integratorIdx : syncIt) {
        auto & self = render.integratorData[integratorIdx];

        switch (self.renderingState) {
          default:
            self.startTime = std::chrono::system_clock::now();
          break;
          case mt::RenderingState::Off: break;
          case mt::RenderingState::AfterChange:
            if (self.bufferCleared) {
              self.bufferCleared = false;
              self.startTime = std::chrono::system_clock::now();
            }
          break;
        }
      }

      // -- render synced integrators
      #pragma omp parallel for collapse(2)
      for (size_t x = 0; x < resolution.x; ++ x)
      for (size_t y = 0; y < resolution.y; ++ y) {

        glm::vec2 uv = glm::vec2(x, y) / glm::vec2(resolution.x, resolution.y);
        uv.x = 1.0f - uv.x; // flip X axis for image
        uv = (uv - glm::vec2(0.5f)) * 2.0f;
        uv.y *= resolutionAspectRatio;

        // TODO realtime probably should have hardcoded UV offsets to be
        //      consistent
        auto const eye =
          plugin.camera.Dispatch(
            plugin.random, render.camera
          , render.integratorData[syncIt[0]].imageResolution
          , uv
          );

        // apply raycast to surface for all synced integrators
        auto const surface =
          mt::core::Raycast(scene, plugin, eye.origin, eye.direction, -1ul);

        for (auto const integratorIdx : syncIt) {
          auto & self = render.integratorData[integratorIdx];

          auto & pixel = self.mappedImageTransitionBuffer[y*resolution.x + x];

          auto pixelResults =
            plugin
              .integrators[integratorIdx]
              .DispatchRealtime(uv, surface, scene, plugin, self);

          pixel = pixelResults.color;
        }
      }

      // -- apply image copy & set rendering finished
      for (auto const integratorIdx : syncIt) {
        auto & self = render.integratorData[integratorIdx];
        mt::core::DispatchImageCopy(self, 0ul, 0ul, resolution.x, resolution.y);
        self.endTime = std::chrono::system_clock::now();
        self.renderingFinished = true;
      }

      continue;
    }

    // -- compute offline renderer
    for (auto const integratorIdx : syncIt) {
      auto & self = render.integratorData[integratorIdx];

      switch (self.renderingState) {
        default: continue;
        case mt::RenderingState::Off: continue;
        case mt::RenderingState::AfterChange:
          if (self.bufferCleared) {
            self.bufferCleared = false;
            continue;
          }
        [[fallthrough]];
        case mt::RenderingState::OnChange:
          ++ self.dispatchedCycles;
        break;
        case mt::RenderingState::OnAlways:
          mt::core::Clear(self);
        break;
      }

      // start the timer if(f) first iteration
      if (self.dispatchedCycles == 1ul) {
        self.startTime = std::chrono::system_clock::now();
      }

      if (
          self.imageResolution.x * self.imageResolution.y
       != self.mappedImageTransitionBuffer.size()
      ) {
        spdlog::critical(
          "Image resolution ({}, {}) mismatch with buffer size {}"
        , self.imageResolution.x, self.imageResolution.y
        , self.mappedImageTransitionBuffer.size()
        );
        self.renderingState = mt::RenderingState::Off;
        continue;
      }

      glm::u16vec2 minRange = glm::uvec2(0);
      glm::u16vec2 maxRange = self.imageResolution;
      const bool realtime = plugin.integrators[integratorIdx].RealTime();

      // -- update block information

      // dispatch min/max range if it has been overriden
      if (self.hasDispatchOverride) {
        minRange = self.dispatchBegin;
        maxRange = self.dispatchEnd;
      } else if (!realtime) {
        ::BlockIterate(self, minRange, maxRange);
      }

      ::DispatchBlockRegion(
        scene, render, plugin, integratorIdx
      , minRange.x, minRange.y, maxRange.x, maxRange.y
      , 1, 1
      , 1
      , !self.hasDispatchOverride
      );

      // prepare kernels
      ::PrepareKernels(render, self, scene, plugin);

      // apply kernels
      for (auto const & kernelDispatch : self.kernelDispatchers) {
        switch (kernelDispatch.timing) {
          default: break;
          case mt::KernelDispatchTiming::Start:
            // TODO this has to be done before doing any dispatches
          break;
          case mt::KernelDispatchTiming::Preview:
            if (self.generatePreviewOutput) {
              spdlog::info("preview output");
              plugin
                .kernels[kernelDispatch.dispatchPluginIdx]
                .ApplyKernel(
                  render, plugin, self
                , make_span(self.mappedImageTransitionBuffer)
                , make_span(self.previewMappedImageTransitionBuffer)
                );
            }
          break;
          case mt::KernelDispatchTiming::All:
          break;
          case mt::KernelDispatchTiming::Last:
            if (self.renderingFinished) {
              plugin
                .kernels[kernelDispatch.dispatchPluginIdx]
                .ApplyKernel(
                    render, plugin, self
                  , make_span(self.mappedImageTransitionBuffer)
                  , make_span(self.mappedImageTransitionBuffer)
                );
            }
          break;
        }
      }

      // apply image copy
      mt::core::DispatchImageCopy(
        self
      , minRange.x, maxRange.x, minRange.y, maxRange.y
      );

      // clear out preview output (must be after image copy)
      self.generatePreviewOutput = false;

      ::BlockCollectFinishedPixels(self, plugin.integrators[integratorIdx]);
    }
  }
}

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


  { // -- synced integrator displa
    ImGui::Begin("synced integrator display");
    for (size_t idx = 0ul; idx < ::syncedIntegrators.size(); ++ idx) {
      ImGui::Separator();
      ImGui::Text("---- sync group %lu ---------------", idx);
      for (auto const integratorIdx : ::syncedIntegrators[idx]) {
        ImGui::Text("\t%s", plugin.integrators[integratorIdx].PluginLabel());
      }
    }
    ImGui::End();
  }

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

} // extern C
