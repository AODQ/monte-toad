#pragma once

// Header-only struct definitions intended to be used by all plugins &
// monte-toad library, specifically for cross-plugin/host communication.
// hence its inclusion in the cr library itself though it will probably be moved
// to its own library in the future

#include <glm/glm.hpp>

#include <mt-plugin/enums.hpp>

#include <array>
#include <filesystem>
#include <string>
#include <tuple>

namespace mt { struct DiagnosticInfo; }
namespace mt { struct PluginInfo; }
namespace mt { struct RenderInfo; }
namespace mt { struct Scene; }
namespace mt { struct SurfaceInfo; }

namespace mt {

  struct PluginInfoIntegrator {
    glm::vec3 (*Dispatch)(
      glm::vec2 const & uv, glm::vec2 const & resolution
    , mt::Scene const & scene
    , mt::RenderInfo const & renderInfo
    , mt::PluginInfo const & pluginInfo
    ) = nullptr;

    bool realtime = false; // renders either block-by-block or full-screen
    bool useGpu = false; // TODO , will give GPU texture handle to render to

    mt::PluginType pluginType;
  };

  struct PluginInfoKernel {
    glm::vec3 (*Tonemap)(
      glm::vec2 const & uv, glm::vec2 const & resolution
    , glm::vec3 color
    , mt::RenderInfo const & renderInfo
    , mt::PluginInfo const & pluginInfo
    ) = nullptr;

    glm::vec3 (*Denoise)(
      glm::vec2 uv, glm::vec2 resolution
    , glm::vec3 color
    , mt::RenderInfo const & renderInfo
    , mt::PluginInfo const & pluginInfo
    ) = nullptr;

    mt::PluginType pluginType;
  };

  struct PluginInfoRandom {
    void (*Initialize)() = nullptr;
    void (*Clean)() = nullptr;

    float     (*SampleUniform1)() = nullptr;
    glm::vec2 (*SampleUniform2)() = nullptr;
    glm::vec3 (*SampleUniform3)() = nullptr;

    mt::PluginType pluginType;
  };

  struct PluginInfoMaterial {
    void (*Load)(std::filesystem::path const & filepath) = nullptr;
    void (*Clean)() = nullptr;

    std::tuple<glm::vec3 /*wo*/, float /*pdf*/> (*BsdfSample)(
      mt::PluginInfoRandom & random
    , mt::SurfaceInfo const & surface
    , glm::vec3 const & wi
    ) = nullptr;

    float (*BsdfPdf)(
      mt::SurfaceInfo const & surface
    , glm::vec3 const & wi, glm::vec3 const & wo
    ) = nullptr;

    glm::vec3 (*BsdfFs)(
      mt::Scene const & scene, mt::SurfaceInfo const & surface
    , glm::vec3 const & wi, glm::vec3 const & wo
    ) = nullptr;

    mt::PluginType pluginType;
  };

  struct PluginInfoCamera {
    std::tuple<glm::vec3 /*ori*/, glm::vec3 /*dir*/> (*Dispatch)(
      mt::PluginInfoRandom const & random
    , mt::RenderInfo const & renderInfo
    , glm::vec2 const & uv
    ) = nullptr;

    mt::PluginType pluginType;
  };

  struct PluginInfoUserInterface {
    void (*Dispatch)(
      mt::Scene & scene
    , mt::RenderInfo & renderInfo
    , mt::PluginInfo & pluginInfo
    , mt::DiagnosticInfo & diagnosticInfo
    ) = nullptr;

    mt::PluginType pluginType;
  };

  struct PluginInfo {
    PluginInfoIntegrator integrator;
    PluginInfoKernel kernel; // optional
    PluginInfoMaterial material;
    PluginInfoCamera camera; // optional
    PluginInfoRandom random;
    PluginInfoUserInterface userInterface; //optional
  };

  struct RenderInfo {
    std::string modelFile;
    std::string outputFile;
    std::string environmentMapFile;

    bool viewImageOnCompletion;
    glm::vec3 cameraOrigin { 1.0f, 0.0f, 0.0f };
    glm::vec3 cameraTarget { 0.0f, 0.0f, 0.0f };
    glm::vec3 cameraUpAxis { 0.0f, -1.0f, 0.0f };
    std::array<uint32_t, 2> imageResolution {{ 640, 480 }};
    bool bvhUse = true;
    bool bvhOptimize = true;
    size_t numThreads = 0;
    size_t samplesPerPixel = 1;
    size_t pathsPerSample = 1;
    float cameraFieldOfView = 90.0f;
    bool displayProgress = true;
  };

  struct DiagnosticInfo {
    glm::vec4 * currentFragmentBuffer;
    void * textureHandle;
  };
}
