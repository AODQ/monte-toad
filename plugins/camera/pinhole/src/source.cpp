#include <cr/cr.h>

#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>

#include <mt-plugin/plugin.hpp>

namespace {
glm::vec3 LookAt(glm::vec3 dir, glm::vec2 uv, glm::vec3 up, float fovRadians) {
  glm::vec3
    ww = glm::normalize(dir),
    uu = glm::normalize(glm::cross(ww, up)),
    vv = glm::normalize(glm::cross(uu, ww));
  return
    glm::normalize(
      glm::mat3(uu, vv, ww)
    * glm::vec3(uv.x, uv.y, fovRadians)
    );
}

std::tuple<glm::vec3 /*ori*/, glm::vec3 /*dir*/> Dispatch(
  mt::PluginInfoRandom const & random
, mt::RenderInfo const & render
, glm::u16vec2 imageResolution
, glm::vec2 uv
) {
  // anti aliasing
  uv +=
      (glm::vec2(-0.5f) + random.SampleUniform2())
    / glm::vec2(imageResolution[0], imageResolution[1])
  ;

  std::tuple<glm::vec3, glm::vec3> results;
  std::get<0>(results) = render.cameraOrigin;
  std::get<1>(results) =
    ::LookAt(
      render.cameraDirection
    , uv
    , render.cameraUpAxis
    , glm::radians(180.0f - render.cameraFieldOfView)
    );
  return results;
}
}

CR_EXPORT int cr_main(struct cr_plugin * ctx, enum cr_op operation) {
  // return immediately if an update, this won't do anything
  if (operation == CR_STEP || operation == CR_UNLOAD) { return 0; }
  if (!ctx || !ctx->userdata) { return 0; }

  auto & camera = *reinterpret_cast<mt::PluginInfoCamera*>(ctx->userdata);

  switch (operation) {
    case CR_LOAD:
      camera.Dispatch = &::Dispatch;
      camera.pluginType = mt::PluginType::Camera;
      camera.pluginLabel = "pinhole camera";
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  return 0;
}