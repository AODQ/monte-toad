#include <monte-toad/core/camerainfo.hpp>

#include <monte-toad/core/renderinfo.hpp>
#include <mt-plugin/plugin.hpp>

void mt::core::UpdateCamera(
  mt::PluginInfo const & plugin
, mt::core::RenderInfo & render
) {
  if (plugin.camera.UpdateCamera)
    { plugin.camera.UpdateCamera(render.camera); }
  render.ClearImageBuffers();
}
