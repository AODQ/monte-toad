#pragma once

// Header-only struct definitions intended to be used by all plugins &
// monte-toad library, specifically for cross-plugin/host communication.
// hence its inclusion in the cr library itself though it will probably be moved
// to its own library in the future

#include <mt-plugin/enums.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
    #include <glm/fwd.hpp>
#pragma GCC diagnostic pop

#include <optional>
#include <vector>

// TODO implement functional instead of ptrs

namespace mt::core { struct Any; }
namespace mt::core { struct BsdfSampleInfo; }
namespace mt::core { struct BvhIntersection; }
namespace mt::core { struct CameraInfo; }
namespace mt::core { struct IntegratorData; }
namespace mt::core { struct RenderInfo; }
namespace mt::core { struct Scene; }
namespace mt::core { struct SurfaceInfo; }
namespace mt::core { struct Triangle; }
namespace mt::core { struct TriangleMesh; }
namespace mt::debugutil { struct IntegratorPathUnit; }
namespace mt { enum struct BsdfTypeHint : uint8_t; }
namespace mt { struct PluginInfo; }
template <typename T> struct span;

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
    , mt::PluginInfo const & plugin
    , mt::core::IntegratorData const & integratorData
    , void (*debugPathRecorder)(mt::debugutil::IntegratorPathUnit)
    ) = nullptr;

    PixelInfo (*DispatchRealtime)(
      glm::vec2 const & uv
    , mt::core::SurfaceInfo const & surface
    , mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    , mt::core::IntegratorData const & integratorData
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

  struct PluginInfoAccelerationStructure {
    mt::core::Any (*Construct)(
      mt::core::TriangleMesh && triangleMesh
    ) = nullptr;

    std::optional<mt::core::BvhIntersection> (*IntersectClosest)(
      mt::core::Any const & self
    , glm::vec3 const & ori
    , glm::vec3 const & dir
    , size_t const ignoredTriangle
    );

    mt::core::Triangle (*GetTriangle)(
      mt::core::Any const & self, size_t const triangleIdx
    );

    void (*UiUpdate)(
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)() = nullptr;
    char const * (*PluginLabel)() = nullptr;
  };

  struct PluginInfoKernel {
    void (*ApplyKernel)(
      mt::core::RenderInfo & render
    , mt::PluginInfo const & plugin
    , mt::core::IntegratorData & integratorData
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

  struct PluginInfoBsdf {
    void (*Allocate)(mt::core::Any & userdata) = nullptr;

    void (*UiUpdate)(
      mt::core::Any & userdata
    , mt::core::RenderInfo & render
    , mt::core::Scene & scene
    ) = nullptr;

    mt::core::BsdfSampleInfo (*BsdfSample)(
      mt::core::Any const & data, float const indexOfRefraction
    , mt::PluginInfoRandom const & random
    , mt::core::SurfaceInfo const & surface
    ) = nullptr;

    float (*BsdfPdf)(
      mt::core::Any const & data, float const indexOfRefraction
    , mt::core::SurfaceInfo const & surface
    , glm::vec3 const & wo
    ) = nullptr;

    glm::vec3 (*AlbedoApproximation)(
      mt::core::Any const & data, float const indexOfRefraction
    , mt::core::SurfaceInfo const & surface
    );

    glm::vec3 (*BsdfFs)(
      mt::core::Any const & data, float const indexOfRefraction
    , mt::core::SurfaceInfo const & surface
    , glm::vec3 const & wo
    ) = nullptr;

    // TODO consider changing triangle to mesh index since either the entire
    //      mesh is an emitter or it's not
    bool (*IsEmitter)(
      mt::core::Any const & data
    , mt::core::Triangle const triangle
    );

    // helps determine what type of bsdf it is to help with bsdf picking in
    // material
    mt::BsdfTypeHint (*BsdfType)();

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
  };

  struct PluginInfoMaterial {
    void (*Allocate)(mt::core::Any & userdata) = nullptr;

    bool (*IsEmitter)(
      mt::core::SurfaceInfo const & surface
    , mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    );

    mt::core::BsdfSampleInfo (*Sample)(
      mt::core::SurfaceInfo const & surface
    , mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    );

    float (*Pdf)(
      mt::core::SurfaceInfo const & surface
    , mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    , glm::vec3 const & wo
    , bool const reflection
    , size_t const componentIdx
    );

    float (*IndirectPdf)(
      mt::core::SurfaceInfo const & surface
    , mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    , glm::vec3 const & wo
    );

    glm::vec3 (*EmitterFs)(
      mt::core::SurfaceInfo const & surface
    , mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    );

    glm::vec3 (*BsdfFs)(
      mt::core::SurfaceInfo const & surface
    , mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    , glm::vec3 const & wo
    );

    glm::vec3 (*AlbedoApproximation)(
      mt::core::SurfaceInfo const & surface
    , mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
    );

    void (*UiUpdate)(
      mt::core::Scene & scene
    , mt::core::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType (*PluginType)();
    char const * (*PluginLabel)();
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
    void (*DispatchRender)(
      mt::core::RenderInfo & render
    , mt::core::Scene const & scene
    , mt::PluginInfo const & plugin
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
    std::vector<PluginInfoBsdf> bsdfs;
    std::vector<PluginInfoKernel> kernels;
    PluginInfoAccelerationStructure accelerationStructure;
    PluginInfoMaterial material;
    PluginInfoCamera camera; // optional
    PluginInfoRandom random;
    PluginInfoUserInterface userInterface; //optional
  };
}
