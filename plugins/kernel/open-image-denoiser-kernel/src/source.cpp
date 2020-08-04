// -- open image denoiser kernel

#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <mt-plugin/plugin.hpp>

#include <OpenImageDenoise/oidn.h>

extern "C" {

void Apply(
  glm::vec2 const & uv
, glm::vec3 & color
, mt::core::RenderInfo const & render
, mt::PluginInfo const & plugin
, mt::core::IntegratorData const & integratorData
) {
  OIDNDevice device = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
  oidnCommitDevice(device);

  /* OIDNFilter filter; */

  oidnReleaseDevice(device);
}

}
