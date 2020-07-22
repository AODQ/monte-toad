#include <monte-toad/camerainfo.hpp>

#include <monte-toad/renderinfo.hpp>
#include <mt-plugin/plugin.hpp>

void mt::UpdateCamera(
  mt::PluginInfo const & plugin
, mt::RenderInfo & render
) {
  if (plugin.camera.UpdateCamera)
    { plugin.camera.UpdateCamera(render.camera); }
  render.ClearImageBuffers();
}
