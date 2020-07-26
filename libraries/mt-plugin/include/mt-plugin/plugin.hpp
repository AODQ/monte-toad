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

namespace mt::core { struct CameraInfo; }
namespace mt::core { struct IntegratorData; }
namespace mt::core { struct RenderInfo; }
namespace mt::core { struct Scene; }
namespace mt::core { struct SurfaceInfo; }
namespace mt::core { struct Triangle; }
namespace mt::debugutil { struct IntegratorPathUnit; }
namespace mt { struct PluginInfo; }

namespace mt {

  struct PixelInfo {
    glm::vec3 color;
    bool valid;
  };

  struct PluginInfoIntegrator {
    PixelInfo (*Dispatch)(
      glm::vec2 const & uv
    , mt::core::Scene const & scene
    , mt::core::CameraInfo const & camera
    , mt::PluginInfo const & pluginInfo
    , mt::core::IntegratorData const & integratorData
    , void (*debugPathRecorder)(mt::debugutil::IntegratorPathUnit)
    ) = nullptr;

    void (*UiUpdate)(
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
    , mt::PluginInfo const & plugin
    , mt::core::IntegratorData & integratorData
    ) = nullptr;

    bool (*RealTime)();
    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoKernel {
    glm::vec3 (*Tonemap)(
      glm::vec2 const & uv
    , glm::vec3 color
    , mt::core::RenderInfo const & renderInfo
    , mt::PluginInfo const & pluginInfo
    , mt::core::IntegratorData const & integratorData
    ) = nullptr;

    glm::vec3 (*Denoise)(
      glm::vec2 uv, glm::vec2 resolution
    , glm::vec3 color
    , mt::core::RenderInfo const & renderInfo
    , mt::PluginInfo const & pluginInfo
    ) = nullptr;

    void (*UiUpdate)(
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
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
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
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
    void (*Load)(mt::PluginInfoMaterial & self, mt::core::Scene &) = nullptr;

    BsdfSampleInfo (*BsdfSample)(
      mt::PluginInfoMaterial const & self
    , mt::PluginInfoRandom const & random
    , mt::core::SurfaceInfo const & surface
    ) = nullptr;

    float (*BsdfPdf)(
      mt::PluginInfoMaterial const & self
    , mt::core::SurfaceInfo const & surface
    , glm::vec3 const & wo
    ) = nullptr;

    glm::vec3 (*BsdfFs)(
      mt::PluginInfoMaterial const & self
    , mt::core::SurfaceInfo const & surface
    , glm::vec3 const & wo
    ) = nullptr;

    bool (*IsEmitter)(
      mt::PluginInfoMaterial const & self
    , mt::core::Scene const & scene
    , mt::core::Triangle const & triangle
    );

    void (*UiUpdate)(
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
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
    , mt::core::CameraInfo const & camera
    , glm::u16vec2 imageResolution
    , glm::vec2 uv
    ) = nullptr;

    // optional, used to update plugin data when camera changes
    void (*UpdateCamera)(mt::core::CameraInfo const & camera) = nullptr;

    // optional, but used in order to draw debug rendering lines
    glm::vec2 (*WorldCoordToUv)(
      mt::core::CameraInfo const & camera, glm::vec3 worldCoord
    ) = nullptr;

    void (*UiUpdate)(
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoUserInterface {
    void (*Dispatch)(
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoEmitter {
    mt::PixelInfo (*SampleLi)(
      mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    , mt::core::SurfaceInfo const & surface
    , glm::vec3 & wo
    , float & pdf
    ) = nullptr;

    mt::PixelInfo (*SampleWo)(
      mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    , mt::core::SurfaceInfo const & surface
    , glm::vec3 const & wo
    , float & pdf
    ) = nullptr;

    void (*Precompute)(
      mt::core::Scene const & scene
    , mt::core::RenderInfo const & render
    , mt::PluginInfo const & plugin
    );

    void (*UiUpdate)(
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    bool (*IsSkybox)();

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoDispatcher {
    void (*DispatchBlockRegion)(
      mt::core::Scene const & scene
    , mt::core::RenderInfo & render
    , mt::PluginInfo const & pluginInfo

    , size_t integratorIdx
    , size_t const minX, size_t const minY
    , size_t const maxX, size_t const maxY
    , size_t strideX, size_t strideY
    , size_t internalIterator
    ) = nullptr;

    void (*UiUpdate)(
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
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
