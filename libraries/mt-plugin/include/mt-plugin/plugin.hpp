#pragma once

// Header-only struct definitions intended to be used by all plugins &
// monte-toad library, specifically for cross-plugin/host communication.
// hence its inclusion in the cr library itself though it will probably be moved
// to its own library in the future

#include <glm/glm.hpp>

#include <mt-plugin/enums.hpp>

#include <array>
#include <functional>
#include <string>
#include <tuple>
#include <vector>

// TODO implement functional instead of ptrs

namespace mt { struct PluginInfo; }
namespace mt { struct RenderInfo; }
namespace mt { struct Scene; }
namespace mt { struct IntegratorData; }
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
    , mt::RenderInfo const & renderInfo
    , mt::PluginInfo const & pluginInfo
    , mt::IntegratorData const & integratorData
    ) = nullptr;

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    , mt::IntegratorData & integratorData
    ) = nullptr;

    bool realtime = false; // renders either block-by-block or full-screen
    bool useGpu = false; // TODO , will give GPU texture handle to render to

    mt::PluginType pluginType;
    char const * pluginLabel;
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

    mt::PluginType pluginType;
    char const * pluginLabel;
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

    mt::PluginType pluginType;
    char const * pluginLabel;
  };

  struct PluginInfoMaterial {
    void (*Load)(mt::Scene &) = nullptr;

    std::tuple<glm::vec3 /*wo*/, float /*pdf*/> (*BsdfSample)(
      mt::PluginInfoRandom const & random
    , mt::SurfaceInfo const & surface
    ) = nullptr;

    float (*BsdfPdf)(
      mt::SurfaceInfo const & surface
    , glm::vec3 const & wo
    ) = nullptr;

    glm::vec3 (*BsdfFs)(
      mt::Scene const & scene
    , mt::SurfaceInfo const & surface
    , glm::vec3 const & wo
    ) = nullptr;

    bool (*IsEmitter)(mt::Scene const & scene, mt::Triangle const & triangle);

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType pluginType;
    char const * pluginLabel;
  };

  struct PluginInfoCamera {
    std::tuple<glm::vec3 /*ori*/, glm::vec3 /*dir*/> (*Dispatch)(
      mt::PluginInfoRandom const & random
    , mt::RenderInfo const & renderInfo
    , glm::u16vec2 imageResolution
    , glm::vec2 uv
    ) = nullptr;

    void (*UiUpdate)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType pluginType;
    char const * pluginLabel;
  };

  struct PluginInfoUserInterface {
    void (*Dispatch)(
      mt::Scene & scene
    , mt::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) = nullptr;

    mt::PluginType pluginType;
    char const * pluginLabel;
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

    bool isSkybox;

    mt::PluginType pluginType;
    char const * pluginLabel;
  };

  struct PluginInfo {
    std::vector<PluginInfoIntegrator> integrators;
    std::vector<PluginInfoEmitter> emitters;
    PluginInfoKernel kernel; // optional
    PluginInfoMaterial material;
    PluginInfoCamera camera; // optional
    PluginInfoRandom random;
    PluginInfoUserInterface userInterface; //optional
  };
}
