#pragma once

namespace mt { struct PluginInfo; }
namespace mt::core { struct RenderInfo; }

namespace mt::core {
  struct CameraInfo {
    glm::vec3 origin { 1.0f, 0.0f, 0.0f };
    glm::vec3 direction { 0.0f, 0.0f, 1.0f };
    glm::vec3 upAxis { 0.0f, -1.0f, 0.0f };
    float fieldOfView = 90.0f;
  };

  // updates camera for plugin and clears image buffer
  void UpdateCamera(
    mt::PluginInfo const & plugin
  , mt::core::RenderInfo & render
  );
}
