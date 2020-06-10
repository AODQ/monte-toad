#include <monte-toad/renderinfo.hpp>

#include <monte-toad/engine.hpp>
#include <monte-toad/log.hpp>
#include <mt-plugin/plugin.hpp>

#include <glad/glad.hpp>

void mt::IntegratorData::Clear(bool fast) {
  std::fill(
    this->pixelCountBuffer.begin()
  , this->pixelCountBuffer.end()
  , 0
  );

  std::fill(
    this->mappedImageTransitionBuffer.begin()
  , this->mappedImageTransitionBuffer.end()
  , glm::vec4(0.0f)
  );

  collectedSamples = 0;
  bufferCleared = true;
}

bool mt::IntegratorData::DispatchRender(
  mt::Scene const & scene
, mt::RenderInfo & render
, mt::PluginInfo const & plugin
, size_t integratorIdx
) {
  switch (this->renderingState) {
    default: return false;
    case mt::RenderingState::Off: return false;
    case mt::RenderingState::AfterChange:
      if (this->bufferCleared) {
        this->bufferCleared = false;
        return false;
      }
    [[fallthrough]]
    case mt::RenderingState::OnChange:
      if (this->collectedSamples >= this->samplesPerPixel) { return false; }
      ++ this->collectedSamples;
    break;
    case mt::RenderingState::OnAlways:
      this->Clear(true);
    break;
  }

  if (
      this->imageResolution.x * this->imageResolution.y
   != this->mappedImageTransitionBuffer.size()
  ) {
    spdlog::critical(
      "Image resolution ({}, {}) mismatch with buffer size {}"
    , this->imageResolution.x, this->imageResolution.y
    , this->mappedImageTransitionBuffer.size()
    );
    this->renderingState = mt::RenderingState::Off;
    return false;
  }

  imageStride = 1;

  if (
      !plugin.integrators[integratorIdx].realtime
    && this->collectedSamples == 1
  ) {
    imageStride = 8;
  }

  mt::DispatchEngineBlockRegion(
    scene, render, plugin, integratorIdx
  , 0, 0, this->imageResolution.x, this->imageResolution.y
  , imageStride, imageStride
  );

  return true;
}

void mt::IntegratorData::FlushTransitionBuffer() {
  glFlushMappedBufferRange(
    this->imageTransitionBuffer.handle
  , 0, sizeof(glm::vec4) * this->mappedImageTransitionBuffer.size()
  );
}

void mt::IntegratorData::DispatchImageCopy() {
  glBindTextureUnit(0, this->renderedTexture.handle);
  glBindBufferBase(
    GL_SHADER_STORAGE_BUFFER, 0, this->imageTransitionBuffer.handle
  );
  glBindImageTexture(
    0, this->renderedTexture.handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8
  );

  glUniform1i(0, this->imageStride);

  glDispatchCompute(
    this->imageResolution.x / 8
  , this->imageResolution.y / 8
  , 1
  );
}

void mt::IntegratorData::AllocateGlResources(
  mt::RenderInfo const & renderInfo
) {
  spdlog::info("Allocating gl resources to {}", this->imageResolution);
  size_t const
    imagePixelLength = this->imageResolution.x * this->imageResolution.y
  , imageByteLength  = imagePixelLength * sizeof(glm::vec4)
  ;

  // -- construct transition buffer
  this->imageTransitionBuffer.Construct();
  glNamedBufferStorage(
    this->imageTransitionBuffer.handle
  , imageByteLength
  , nullptr
  , GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT
  );

  this->mappedImageTransitionBuffer =
    span<glm::vec4>(
      reinterpret_cast<glm::vec4*>(
        glMapNamedBufferRange(
          this->imageTransitionBuffer.handle
        , 0, imageByteLength
        , ( GL_MAP_WRITE_BIT
          | GL_MAP_PERSISTENT_BIT
          | GL_MAP_INVALIDATE_RANGE_BIT
          | GL_MAP_INVALIDATE_BUFFER_BIT
          | GL_MAP_FLUSH_EXPLICIT_BIT
          )
        )
      )
    , imagePixelLength
    );

  this->pixelCountBuffer.resize(imagePixelLength);

  // -- construct texture
  this->renderedTexture.Construct(GL_TEXTURE_2D);
  glTextureStorage2D(
    this->renderedTexture.handle
  , 1, GL_RGBA8
  , this->imageResolution.x, this->imageResolution.y
  );

  { // -- set parameters
    auto const & handle = this->renderedTexture.handle;
    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  }

  // clear resources of garbage memory
  this->Clear();
}

void mt::RenderInfo::ClearImageBuffers() {
  for (auto & integrator : this->integratorData)
    { integrator.Clear(); }
}
