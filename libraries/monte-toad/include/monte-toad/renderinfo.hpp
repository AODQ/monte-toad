#pragma once

#include <monte-toad/enum.hpp>
#include <monte-toad/glutil.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/span.hpp>

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

namespace mt { struct RenderInfo; }
namespace mt { struct Scene; }
namespace mt { struct PluginInfo; }

namespace mt {
  struct IntegratorData {
    mt::GlBuffer imageTransitionBuffer;
    span<glm::vec4> mappedImageTransitionBuffer;
    std::vector<uint16_t> pixelCountBuffer;
    mt::GlTexture renderedTexture;

    glm::u16vec2 imageResolution = glm::u16vec2(640, 480);
    mt::AspectRatio imageAspectRatio = mt::AspectRatio::e4_3;
    bool syncAspectRatioToPrimaryIntegrator = false;
    bool syncCameraToPrimaryIntegrator = false;

    mt::RenderingState renderingState = mt::RenderingState::Off;

    size_t samplesPerPixel = 1;
    size_t pathsPerSample = 1;

    bool renderingFinished = false;

    bool imagePixelClicked = false;
    glm::uvec2 imagePixelClickedCoord = glm::uvec2(0, 0);

    bool overrideImGuiImageResolution = false;
    glm::uint16_t imguiImageResolution = 640;

    // -- information used to collect pixel informations over blocks
    std::vector<uint16_t> blockPixelsFinished;
    bool bufferCleared = false;
    size_t dispatchedCycles = 0u;
    size_t imageStride = 1ul;
    size_t blockIterator = 0ul;
    size_t blockInternalIterator = 0ul;
    size_t blockInternalIteratorMax = 1ul;
    size_t blockIteratorStride = 128ul;

    // TODO maybe implement below sometime
    // -- used to 'clean up' the last few pixels; so we can just batch the last
    //    100 or whatever pixels instead of brute forcing them through blocks.
    //    For now this size is controlled by blockIteratorStride but can change
    //    to something else
    std::vector<size_t> unfinishedPixels;
    size_t unfinishedPixelsCount;

    // this is a bit weird but in order to collect the unfinished pixels, a =
    bool collectedUnfinishedPixels;

    glm::uvec2
      manualBlockMin = glm::uvec2(0, 0)
    , manualBlockMax = glm::uvec2(0, 0)
    ;
  };

  void Clear(mt::IntegratorData & self, bool fast = false);

  bool DispatchRender(
    mt::IntegratorData & self
  , mt::Scene const & scene
  , mt::RenderInfo & render
  , mt::PluginInfo const & plugin
  , size_t integratorIdx
  );

  size_t FinishedPixels(mt::IntegratorData & self);
  size_t FinishedPixelsGoal(mt::IntegratorData & self);

  size_t BlockIteratorMax(mt::IntegratorData & self);

  void FlushTransitionBuffer(mt::IntegratorData & self);

  void DispatchImageCopy(mt::IntegratorData & self);

  void AllocateGlResources(
    mt::IntegratorData & self
  , mt::RenderInfo const & renderInfo
  );

  struct RenderInfo {
    std::string modelFile;
    std::string outputFile;
    std::string environmentMapFile;

    bool viewImageOnCompletion;
    glm::vec3 cameraOrigin { 1.0f, 0.0f, 0.0f };
    glm::vec3 cameraDirection { 0.0f, 0.0f, 1.0f };
    glm::vec3 cameraUpAxis { 0.0f, -1.0f, 0.0f };
    size_t numThreads = 0;
    float cameraFieldOfView = 90.0f;
    bool displayProgress = true;

    int32_t lastIntegratorImageClicked = -1;

    void * glfwWindow;

    std::vector<mt::IntegratorData> integratorData;
    size_t primaryIntegrator;

    void ClearImageBuffers();
  };
}
