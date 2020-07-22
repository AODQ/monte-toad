#pragma once

#include <monte-toad/enum.hpp>
#include <monte-toad/glutil.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/span.hpp>

namespace mt {
  struct IntegratorData {
    std::vector<glm::vec4> mappedImageTransitionBuffer;
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
    std::vector<size_t> blockPixelsFinished;
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
}
