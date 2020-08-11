#include <monte-toad/core/renderinfo.hpp>

#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <mt-plugin/plugin.hpp>

#include <glad/glad.hpp>

namespace {

} // anon namespace

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

  std::fill(
    self.previewMappedImageTransitionBuffer.begin()
  , self.previewMappedImageTransitionBuffer.end()
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
  , GL_RGB32F
  , self.imageResolution.x, self.imageResolution.y
  , 0, GL_RGB, GL_FLOAT
  , self.mappedImageTransitionBuffer.data()
  );

  if (!self.realtime && self.blockIterator == self.blockPixelsFinished.size()-1)
  {
    glBindTexture(GL_TEXTURE_2D, self.previewRenderedTexture.handle);
    glTexImage2D(
      GL_TEXTURE_2D
    , 0
    , GL_RGB32F
    , self.imageResolution.x, self.imageResolution.y
    , 0, GL_RGB, GL_FLOAT
    , self.previewMappedImageTransitionBuffer.data()
    );
  }
}

void mt::core::AllocateResources(
  mt::core::IntegratorData & self
, size_t pluginIdx
, mt::PluginInfo const & plugin
) {

  self.pluginIdx = pluginIdx;
  self.realtime = plugin.integrators[pluginIdx].RealTime();

  spdlog::debug("Allocating gl resources to {}", self.imageResolution);
  size_t const
    imagePixelLength = self.imageResolution.x * self.imageResolution.y
  ;

  // -- construct transition buffer
  self.mappedImageTransitionBuffer.resize(imagePixelLength);

  if (!self.realtime) {
    self.previewMappedImageTransitionBuffer.resize(imagePixelLength);
  }

  self.pixelCountBuffer.resize(imagePixelLength);

  // -- construct texture
  self.renderedTexture.Construct(GL_TEXTURE_2D);

  // generate texture memory
  glBindTexture(GL_TEXTURE_2D, self.renderedTexture.handle);
  glTexImage2D(
    GL_TEXTURE_2D
  , 0
  , GL_RGB32F
  , self.imageResolution.x, self.imageResolution.y
  , 0, GL_RGB, GL_FLOAT
  , nullptr
  );

  { // -- set parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  }

  if (!self.realtime) {
    self.previewRenderedTexture.Construct(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, self.previewRenderedTexture.handle);
    glTexImage2D(
      GL_TEXTURE_2D
    , 0
    , GL_RGB32F
    , self.imageResolution.x, self.imageResolution.y
    , 0, GL_RGB, GL_FLOAT
    , nullptr
    );

    { // -- set parameters
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }
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
