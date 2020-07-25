#pragma once

#include <monte-toad/core/camerainfo.hpp>
#include <monte-toad/core/enum.hpp>

#include <array>
#include <string>
#include <vector>

namespace mt::core { struct RenderInfo; }
namespace mt::core { struct IntegratorData; }
namespace mt::core { struct Scene; }
namespace mt::core { struct PluginInfo; }

namespace mt::core {

  void Clear(mt::core::IntegratorData & self);

  bool DispatchRender(
    mt::core::IntegratorData & self
  , mt::core::Scene const & scene
  , mt::core::RenderInfo & render
  , mt::PluginInfo const & plugin
  , size_t integratorIdx
  );

  size_t FinishedPixels(mt::core::IntegratorData & self);
  size_t FinishedPixelsGoal(mt::core::IntegratorData & self);

  size_t BlockIteratorMax(mt::core::IntegratorData & self);

  void FlushTransitionBuffer(mt::core::IntegratorData & self);

  void DispatchImageCopy(mt::core::IntegratorData & self);

  void AllocateGlResources(
    mt::core::IntegratorData & self
  , mt::core::RenderInfo const & renderInfo
  );

  struct RenderInfo {
    std::string modelFile;
    std::string outputFile;
    std::string environmentMapFile;

    bool viewImageOnCompletion;
    size_t numThreads = 0;
    bool displayProgress = true;

    size_t lastIntegratorImageClicked = -1lu;

    void * glfwWindow;

    mt::core::CameraInfo camera;

    std::vector<mt::core::IntegratorData> integratorData;

    size_t primaryDispatcher = 0ul;
    std::array<size_t, Idx(IntegratorTypeHint::Size)> integratorIndices;

    void ClearImageBuffers();
  };
}
