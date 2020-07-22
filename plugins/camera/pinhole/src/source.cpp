#include <monte-toad/camerainfo.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/scene.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <mt-plugin/plugin.hpp>

namespace {

glm::mat3 viewProjectionMatrixDir;
glm::mat4 viewMatrix;
glm::mat4 projectionMatrix;

void CalculateMatrices(mt::CameraInfo const & camera) {
  glm::mat4 const view =
    glm::lookAtLH(
      camera.origin
    , camera.origin - camera.direction
    , camera.upAxis
    );

  float const e = 1.0f/glm::tan(glm::radians(180.0f-camera.fieldOfView)/2.0f);
  float const ar = 1.0f;

  glm::mat4 const proj =
    glm::mat4(
      e,    0.0f,  0.0f, 0.0f
    , 0.0f, e/ar,  0.0f, 0.0f
    , 0.0f, 0.0f, -1.0f, -0.1f
    , 0.0f, 0.0f, -1.0f, 0.0f
    );

  float const e2 = 1.0f/glm::tan(glm::radians(camera.fieldOfView)/2.0f);

  glm::mat4 const proj2 =
    glm::mat4(
      e2,    0.0f,  0.0f, 0.0f
    , 0.0f, e2/ar,  0.0f, 0.0f
    , 0.0f, 0.0f, -1.0f, -0.1f
    , 0.0f, 0.0f, -1.0f, 0.0f
    );

  ::viewMatrix = view;
  ::projectionMatrix = proj2;
  ::viewProjectionMatrixDir = glm::mat3(glm::transpose(view)*proj);
}

glm::vec3 LookAt(mt::CameraInfo const & camera, glm::vec2 uv) {
  return glm::normalize(::viewProjectionMatrixDir * glm::vec3(uv, 1.0f));
}
}

extern "C" {

char const * PluginLabel() { return "pinhole camera"; }
mt::PluginType PluginType() { return mt::PluginType::Camera; }

std::tuple<glm::vec3 /*ori*/, glm::vec3 /*dir*/> Dispatch(
  mt::PluginInfoRandom const & random
, mt::CameraInfo const & camera
, glm::u16vec2 imageResolution
, glm::vec2 uv
) {
  // anti aliasing
  uv +=
      (glm::vec2(-0.5f) + random.SampleUniform2())
    / glm::vec2(imageResolution[0], imageResolution[1])
  ;

  std::tuple<glm::vec3, glm::vec3> results;
  std::get<0>(results) = camera.origin;
  std::get<1>(results) = ::LookAt(camera, uv);
  return results;
}

void UpdateCamera(mt::CameraInfo const & camera) {
  ::CalculateMatrices(camera);
}

glm::vec2 WorldCoordToUv(
  mt::CameraInfo const & camera, glm::vec3 worldCoord
) {
  glm::mat4 view = ::viewMatrix, proj = ::projectionMatrix;

  view[3][0] = -camera.origin.x;
  view[3][1] = -camera.origin.y;
  view[3][2] = -camera.origin.z;

  auto vec = glm::inverse(::projectionMatrix*glm::transpose(::viewMatrix)) * glm::vec4(worldCoord, -1.0f);
  vec /= vec.z;
  glm::vec2 ndc = glm::vec2(vec.x, vec.y);

  return
    glm::clamp(
      ndc*0.5f + glm::vec2(0.5f)
    , glm::vec2(0.0f)
    , glm::vec2(1.0f)
    );
}

} // -- end extern "C"
