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

    mt::RenderingState renderingState = mt::RenderingState::OnChange;

    size_t samplesPerPixel = 1;
    size_t pathsPerSample = 1;

    bool imagePixelClicked;
    glm::uvec2 imagePixelClickedCoord;

    size_t collectedSamples;

    void Clear(bool fast = false);

    bool DispatchRender(
      mt::Scene const & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    , size_t integratorIdx
    );

    void FlushTransitionBuffer();

    void DispatchImageCopy();

    void AllocateGlResources(mt::RenderInfo const & renderInfo);
  };

  struct RenderInfo {
    std::string modelFile;
    std::string outputFile;
    std::string environmentMapFile;

    bool viewImageOnCompletion;
    glm::vec3 cameraOrigin { 1.0f, 0.0f, 0.0f };
    glm::vec3 cameraDirection { 0.0f, 0.0f, 0.0f };
    glm::vec3 cameraUpAxis { 0.0f, -1.0f, 0.0f };
    size_t numThreads = 0;
    float cameraFieldOfView = 90.0f;
    bool displayProgress = true;

    int32_t lastIntegratorImageClicked = -1;

    void * glfwWindow;

    std::vector<mt::IntegratorData> integratorData;

    void ClearImageBuffers();
  };
}
