// -- open image denoiser kernel

#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/span.hpp>
#include <mt-plugin/plugin.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
  #pragma GCC diagnostic ignored "-Warray-bounds"
  #include <OpenImageDenoise/oidn.hpp>
#pragma GCC diagnostic pop

extern "C" {

char const * PluginLabel() { return "open image denoiser kernel"; }
mt::PluginType PluginType() { return mt::PluginType::Kernel; }

void ApplyKernel(
  mt::core::RenderInfo & /*render*/
, mt::PluginInfo const & /*plugin*/
, mt::core::IntegratorData & integratorData
, span<glm::vec3> inputImageBuffer // TODO make this const
, span<glm::vec3> outputImageBuffer
) {

  oidn::DeviceRef device = oidn::newDevice();
  device.commit();

  oidn::FilterRef filter = device.newFilter("RT");

  { // apply albedo/normal optimization
    auto albedoImg =
      integratorData.secondaryIntegratorImagePtrs[
        Idx(mt::IntegratorTypeHint::Albedo)
      ];

    auto normalImg =
      integratorData.secondaryIntegratorImagePtrs[
        Idx(mt::IntegratorTypeHint::Normal)
      ];

    if (albedoImg.data() != nullptr && normalImg.data() != nullptr) {
      filter
        .setImage(
          "albedo"
        , albedoImg.data()
        , oidn::Format::Float3
        , integratorData.imageResolution.x, integratorData.imageResolution.y
        );

      filter
        .setImage(
          "normal"
        , normalImg.data()
        , oidn::Format::Float3
        , integratorData.imageResolution.x, integratorData.imageResolution.y
        );
    }
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

void UiUpdate(
  mt::core::Scene &
, mt::core::RenderInfo &
, mt::core::IntegratorData &
, mt::PluginInfo const &
) {
}

}
