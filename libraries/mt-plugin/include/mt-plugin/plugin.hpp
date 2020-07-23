#pragma once

// Header-only struct definitions intended to be used by all plugins &
// monte-toad library, specifically for cross-plugin/host communication.
// hence its inclusion in the cr library itself though it will probably be moved
// to its own library in the future

#include <mt-plugin/enums.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wvolatile"
  #pragma GCC diagnostic ignored "-Wshadow"
    #include <glm/fwd.hpp>
#pragma GCC diagnostic pop

#include <vector>

// TODO implement functional instead of ptrs

namespace mt::debugutil { struct IntegratorPathUnit; }
namespace mt { struct CameraInfo; }
namespace mt { struct IntegratorData; }
namespace mt { struct PluginInfo; }
namespace mt { struct RenderInfo; }
namespace mt { struct Scene; }
namespace mt { struct SurfaceInfo; }
namespace mt { struct Triangle; }

namespace mt {

  struct PixelInfo {
    glm::vec3 color;
    bool valid;
  };

  struct PluginInfoIntegrator {
    PixelInfo (*Dispatch)(
      glm::vec2 const & uv
    , mt::Scene const & scene
    , mt::CameraInfo const & camera
    , mt::PluginInfo const & pluginInfo
    , mt::IntegratorData const & integratorData
    , void (*debugPathRecorder)(mt::debugutil::IntegratorPathUnit)
    ) = nullptr;

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    , mt::IntegratorData & integratorData
    ) = nullptr;

    bool (*RealTime)();
    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoKernel {
    glm::vec3 (*Tonemap)(
      glm::vec2 const & uv
    , glm::vec3 color
    , mt::RenderInfo const & renderInfo
    , mt::PluginInfo const & pluginInfo
    , mt::IntegratorData const & integratorData
    ) = nullptr;

    glm::vec3 (*Denoise)(
      glm::vec2 uv, glm::vec2 resolution
    , glm::vec3 color
    , mt::RenderInfo const & renderInfo
    , mt::PluginInfo const & pluginInfo
    ) = nullptr;

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoRandom {
    void (*Initialize)() = nullptr;
    void (*Clean)() = nullptr;

    float     (*SampleUniform1)() = nullptr;
    glm::vec2 (*SampleUniform2)() = nullptr;
    glm::vec3 (*SampleUniform3)() = nullptr;

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct BsdfSampleInfo {
    glm::vec3 wo;
    glm::vec3 fs;
    float pdf;
  };

  struct PluginInfoMaterial {
    void (*Load)(mt::PluginInfoMaterial & self, mt::Scene &) = nullptr;

    BsdfSampleInfo (*BsdfSample)(
      mt::PluginInfoMaterial const & self
    , mt::PluginInfoRandom const & random
    , mt::SurfaceInfo const & surface
    ) = nullptr;

    float (*BsdfPdf)(
      mt::PluginInfoMaterial const & self
    , mt::SurfaceInfo const & surface
    , glm::vec3 const & wo
    ) = nullptr;

    glm::vec3 (*BsdfFs)(
      mt::PluginInfoMaterial const & self
    , mt::SurfaceInfo const & surface
    , glm::vec3 const & wo
    ) = nullptr;

    bool (*IsEmitter)(
      mt::PluginInfoMaterial const & self
    , mt::Scene const & scene
    , mt::Triangle const & triangle
    );

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();

    void * userdata = nullptr;
  };

  struct CameraDispatchInfo {
    glm::vec3 origin;
    glm::vec3 direction;
  };

  struct PluginInfoCamera {
    CameraDispatchInfo (*Dispatch)(
      mt::PluginInfoRandom const & random
    , mt::CameraInfo const & camera
    , glm::u16vec2 imageResolution
    , glm::vec2 uv
    ) = nullptr;

    // optional, used to update plugin data when camera changes
    void (*UpdateCamera)(mt::CameraInfo const & camera) = nullptr;

    // optional, but used in order to draw debug rendering lines
    glm::vec2 (*WorldCoordToUv)(
      mt::CameraInfo const & camera, glm::vec3 worldCoord
    ) = nullptr;

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoUserInterface {
    void (*Dispatch)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoEmitter {
    mt::PixelInfo (*SampleLi)(
      mt::Scene const & scene
    , mt::PluginInfo const & plugin
    , mt::SurfaceInfo const & surface
    , glm::vec3 & wo
    , float & pdf
    ) = nullptr;

    mt::PixelInfo (*SampleWo)(
      mt::Scene const & scene
    , mt::PluginInfo const & plugin
    , mt::SurfaceInfo const & surface
    , glm::vec3 const & wo
    , float & pdf
    ) = nullptr;

    void (*Precompute)(
      mt::Scene const & scene
    , mt::RenderInfo const & render
    , mt::PluginInfo const & plugin
    );

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    bool (*IsSkybox)();

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoDispatcher {
    void (*DispatchBlockRegion)(
      mt::Scene const & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & pluginInfo

    , size_t integratorIdx
    , size_t const minX, size_t const minY
    , size_t const maxX, size_t const maxY
    , size_t strideX, size_t strideY
    ) = nullptr;

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)() = nullptr;
    char const * (*PluginLabel)() = nullptr;
  };

  struct PluginInfo {
    std::vector<PluginInfoIntegrator> integrators;
    std::vector<PluginInfoEmitter> emitters;
    std::vector<PluginInfoDispatcher> dispatchers;
    PluginInfoKernel kernel; // optional
    PluginInfoMaterial material;
    PluginInfoCamera camera; // optional
    PluginInfoRandom random;
    PluginInfoUserInterface userInterface; //optional
  };
}
