#pragma once

#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/glutil.hpp>
#include <monte-toad/core/kerneldispatchinfo.hpp>
#include <monte-toad/core/span.hpp>

#include <vector>
#include <chrono>

namespace mt::core { struct KernelDispatchInfo; }

namespace mt::core {
  struct IntegratorData {
    std::vector<glm::vec3> mappedImageTransitionBuffer;
    std::vector<uint16_t> pixelCountBuffer;
    mt::core::GlTexture renderedTexture;

    std::vector<glm::vec3> previewMappedImageTransitionBuffer;
    mt::core::GlTexture previewRenderedTexture;

    // used to build integrators that are necessary
    std::array<std::vector<glm::vec3>, Idx(mt::IntegratorTypeHint::Size)> 
      secondaryIntegratorImages;

    // pointers to either our own images or other integrators that have been
    // synced to this integrator
    std::array<span<glm::vec3>, Idx(mt::IntegratorTypeHint::Size)>
      secondaryIntegratorImagePtrs;

    size_t pluginIdx;
    bool realtime;

    std::vector<mt::core::KernelDispatchInfo> kernelDispatchers;

    auto HasPreview() -> bool;

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
    std::vector<size_t> blockPixelsFinished;
    bool bufferCleared = false;
    size_t dispatchedCycles = 0u;
    size_t blockIterator = 0ul;
    size_t blockIteratorStride = 128ul;

    // TODO this could easily be a plugin too
    mt::IntegratorDispatchType
      previewDispatchType = mt::IntegratorDispatchType::StrideBlock
    , dispatchType        = mt::IntegratorDispatchType::FillBlockCw
    ;

    // dispatch variables for FillBlowCw, though I could maybe implement the
    // filling process without storing state in the future
    size_t fillBlockLayer = 1ul;
    size_t fillBlockLeg = 0ul;

    bool previewDispatch = true;
    bool generatePreviewOutput = false;

    // dispatch variables for override; this could easily be accomplised if
    // this iteration stuff were a plugin tho
    bool hasDispatchOverride = false;
    glm::u16vec2 dispatchBegin = glm::u16vec2(0), dispatchEnd = glm::u16vec2(0);

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

    std::chrono::time_point<std::chrono::system_clock>
      startTime, endTime;
  };
}
