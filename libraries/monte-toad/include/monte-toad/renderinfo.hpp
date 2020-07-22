#pragma once

#include <monte-toad/camerainfo.hpp>
#include <monte-toad/integratordata.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/span.hpp>

#include <array>
#include <string>
#include <vector>

namespace mt { struct RenderInfo; }
namespace mt { struct Scene; }
namespace mt { struct PluginInfo; }

namespace mt {

  void Clear(mt::IntegratorData & self);

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
    size_t numThreads = 0;
    bool displayProgress = true;

    int32_t lastIntegratorImageClicked = -1;

    void * glfwWindow;

    mt::CameraInfo camera;

    std::vector<mt::IntegratorData> integratorData;

    size_t primaryDispatcher = 0ul;
    std::array<size_t, Idx(IntegratorTypeHint::Size)> integratorIndices;

    void ClearImageBuffers();
  };
}
