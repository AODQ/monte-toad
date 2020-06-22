#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>

#include <mt-plugin/plugin.hpp>

extern "C" {

char const * PluginLabel() { return "pinhole camera"; }
mt::PluginType PluginType() { return mt::PluginType::Camera; }

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

} // -- end extern "C"
