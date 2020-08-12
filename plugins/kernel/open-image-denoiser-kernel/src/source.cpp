// -- open image denoiser kernel

#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/span.hpp>
#include <mt-plugin/plugin.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow -Warray-bounds"
  #include <OpenImageDenoise/oidn.hpp>
#pragma GCC diagnostic pop

extern "C" {

char const * PluginLabel() { return "open image denoiser kernel"; }
mt::PluginType PluginType() { return mt::PluginType::Kernel; }

void ApplyKernel(
  mt::core::RenderInfo & render
, mt::PluginInfo const & /*plugin*/
, mt::core::IntegratorData & integratorData
, span<glm::vec3> inputImageBuffer
, span<glm::vec3> outputImageBuffer
) {

  oidn::DeviceRef device = oidn::newDevice();
  device.commit();

  oidn::FilterRef filter = device.newFilter("RT");

  auto const albedoIdx =
    render.integratorIndices[Idx(mt::IntegratorTypeHint::Albedo)];
  auto const normalIdx =
    render.integratorIndices[Idx(mt::IntegratorTypeHint::Normal)];

  if (albedoIdx != -1lu) {
    auto & data = render.integratorData[albedoIdx];
    filter
      .setImage(
        "albedo"
      , data.mappedImageTransitionBuffer.data()
      , oidn::Format::Float3
      , data.imageResolution.x, data.imageResolution.y
      );
  }

  if (normalIdx != -1lu) {
    auto & data = render.integratorData[normalIdx];
    filter
      .setImage(
        "normal"
      , data.mappedImageTransitionBuffer.data()
      , oidn::Format::Float3
      , data.imageResolution.x, data.imageResolution.y
      );
  }

  filter
    .setImage(
      "color", inputImageBuffer.data()
    , oidn::Format::Float3
    , integratorData.imageResolution.x, integratorData.imageResolution.y
    );

  filter
    .setImage(
      "output", outputImageBuffer.data()
    , oidn::Format::Float3
    , integratorData.imageResolution.x, integratorData.imageResolution.y
    );

  filter.set("hdr", false);
  filter.commit();

  filter.execute();

  char const * errorMsg;
  if (device.getError(errorMsg) != oidn::Error::None)
    { spdlog::error("{}", errorMsg); }
}

}
