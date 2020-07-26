#include <monte-toad/core/renderinfo.hpp>

#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <mt-plugin/plugin.hpp>

#include <glad/glad.hpp>

namespace {

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

  { // perform iteration
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
        return;
      }

      self.blockIterator =
        (self.blockIterator+1) % self.blockPixelsFinished.size();
    }
  }
}
} // namespace

void mt::core::Clear(mt::core::IntegratorData & self) {
  std::fill(
    self.pixelCountBuffer.begin()
  , self.pixelCountBuffer.end()
  , 0
  );

  std::fill(
    self.mappedImageTransitionBuffer.begin()
  , self.mappedImageTransitionBuffer.end()
  , glm::vec4(0.0f)
  );

  self.dispatchedCycles = 0;
  self.bufferCleared = true;
  self.blockIterator = 0ul;
  self.renderingFinished = false;

  // clear block samples
  self.blockPixelsFinished.resize(mt::core::BlockIteratorMax(self));
  std::fill(
    self.blockPixelsFinished.begin(),
    self.blockPixelsFinished.end(),
    0ul
  );

  // clear unfinishedPixels
  self.unfinishedPixelsCount = 0u;

  if (self.renderingState == mt::RenderingState::Off) {
    self.renderingFinished = true;
  }
}

bool mt::core::DispatchRender(
  mt::core::IntegratorData & self
, mt::core::Scene const & scene
, mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
, size_t integratorIdx
) {
  if (self.renderingFinished) { return false; }
  if (plugin.dispatchers.size() == 0) { return false; }
  switch (self.renderingState) {
    default: return false;
    case mt::RenderingState::Off: return false;
    case mt::RenderingState::AfterChange:
      if (self.bufferCleared) {
        self.bufferCleared = false;
        return false;
      }
    [[fallthrough]];
    case mt::RenderingState::OnChange:
      ++ self.dispatchedCycles;
    break;
    case mt::RenderingState::OnAlways:
      mt::core::Clear(self);
    break;
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
    return false;
  }

  // set image stride on first pass otherwise take care of iteration
  self.imageStride = 1;
  glm::u16vec2 minRange = glm::uvec2(0);
  glm::u16vec2 maxRange = self.imageResolution;
  const bool realtime = plugin.integrators[integratorIdx].RealTime();
  bool const quickLowResRender = !realtime && self.dispatchedCycles == 1;
  if (!realtime) {
    switch (self.dispatchedCycles) {
      case 1:
        if (self.imageResolution.x > 1024) {
          self.imageStride = 16;
        } else {
          self.imageStride = 8;
        }
      break;
      default:
        ::BlockIterate(self, minRange, maxRange);
      break;
    }
  }

  plugin
    .dispatchers[render.primaryDispatcher]
    .DispatchBlockRegion(
      scene, render, plugin, integratorIdx
    , minRange.x, minRange.y, maxRange.x, maxRange.y
    , self.imageStride, self.imageStride
    , (realtime || quickLowResRender) ? 1 : self.blockInternalIteratorMax
    );

  // blit transition buffer for 8 pixels, thus generating an 8x lower resolution
  //  image without gaps
  if (quickLowResRender) {
    #pragma omp parallel for
    for (size_t x = minRange.x; x < maxRange.x; ++ x)
    for (size_t y = minRange.y; y < maxRange.y; ++ y) {
      auto const yy = y - (y % self.imageStride);
      auto const xx = x - (x % self.imageStride);
      self.mappedImageTransitionBuffer[y*self.imageResolution.x + x] =
          self.mappedImageTransitionBuffer[yy*self.imageResolution.x + xx];
    }
  }

  ::BlockCollectFinishedPixels(self, plugin.integrators[integratorIdx]);

  // apply image copy
  mt::core::DispatchImageCopy(
    self
  , minRange.x, maxRange.x, minRange.y, maxRange.y
  );

  return true;
}

size_t mt::core::FinishedPixels(mt::core::IntegratorData & self) {
  size_t finishedPixels = 0;
  for (auto & i : self.blockPixelsFinished) { finishedPixels += i; }
  return finishedPixels;
}

size_t mt::core::FinishedPixelsGoal(mt::core::IntegratorData & self) {
  auto const & resolution = self.imageResolution;
  auto const & stride = self.blockIteratorStride;
  // find the next multiple of N (N-1 => N, N => N, N+1 => N*2)
  return
    static_cast<size_t>(resolution.x + ((stride-resolution.x) % stride))
  * static_cast<size_t>(resolution.y + ((stride-resolution.y) % stride))
  ;
}

size_t mt::core::BlockIteratorMax(mt::core::IntegratorData & self) {
  // have to apply ceil seperately on each axis
  auto const & resolution = self.imageResolution;
  auto const & stride = self.blockIteratorStride;
  return
    static_cast<size_t>(std::ceil(resolution.x/static_cast<float>(stride)))
  * static_cast<size_t>(std::ceil(resolution.y/static_cast<float>(stride)));
}

void mt::core::DispatchImageCopy(
  mt::core::IntegratorData & self
  , size_t, size_t, size_t, size_t
) {
  glBindTexture(GL_TEXTURE_2D, self.renderedTexture.handle);
  glTexImage2D(
    GL_TEXTURE_2D
  , 0
  , GL_RGBA32F
  , self.imageResolution.x, self.imageResolution.y
  , 0, GL_RGBA, GL_FLOAT
  , self.mappedImageTransitionBuffer.data()
  );
}

void mt::core::AllocateResources(mt::core::IntegratorData & self) {
  spdlog::debug("Allocating gl resources to {}", self.imageResolution);
  size_t const
    imagePixelLength = self.imageResolution.x * self.imageResolution.y
  ;

  // -- construct transition buffer
  self.mappedImageTransitionBuffer.resize(imagePixelLength);

  self.pixelCountBuffer.resize(imagePixelLength);

  // -- construct texture
  self.renderedTexture.Construct(GL_TEXTURE_2D);

  // generate texture memory
  glBindTexture(GL_TEXTURE_2D, self.renderedTexture.handle);
  glTexImage2D(
    GL_TEXTURE_2D
  , 0
  , GL_RGBA32F
  , self.imageResolution.x, self.imageResolution.y
  , 0, GL_RGBA, GL_FLOAT
  , nullptr
  );

  { // -- set parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  }

  // set unfinishedPixels
  self.unfinishedPixels.resize(self.blockIteratorStride);

  // clear resources of garbage memory
  mt::core::Clear(self);
}

void mt::core::RenderInfo::ClearImageBuffers() {
  for (auto & integrator : this->integratorData)
    { mt::core::Clear(integrator); }
}
