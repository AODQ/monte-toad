#include <monte-toad/scene.hpp>
#include <mt-plugin/plugin.hpp>

#include <cr/cr.h>
#include <glm/glm.hpp>
#include <imgui/imgui.hpp>
#include <spdlog/spdlog.h>

#include <chrono>

namespace {
void Dispatch(
  mt::Scene & scene
, mt::RenderInfo & renderInfo
, mt::PluginInfo & pluginInfo
) {
  if (!(ImGui::Begin("Plugin Info"))) { return; }
  ImGui::Text(
    "Image Resolution (%u, %u)"
  , renderInfo.imageResolution[0], renderInfo.imageResolution[1]
  );

  ImGui::Checkbox("use BVH", &renderInfo.bvhUse);
  ImGui::Checkbox("optimize BVH", &renderInfo.bvhOptimize);

  ImGui::Text("samples per pixel %lu", renderInfo.samplesPerPixel);
  ImGui::Text("paths per sample %lu", renderInfo.pathsPerSample);

  float
    min = glm::min(scene.bboxMin.x, glm::min(scene.bboxMin.y, scene.bboxMin.z))
  , max = glm::max(scene.bboxMax.x, glm::max(scene.bboxMax.y, scene.bboxMax.z));

  // allow viewing outside of min/max bounds
  min -= glm::abs(min)*1.5f;
  max += glm::abs(max)*1.5f;

  ImGui::SliderFloat3("Origin", &renderInfo.cameraOrigin.x, min, max);
  ImGui::SliderFloat3("Target", &renderInfo.cameraTarget.x, min, max);
  ImGui::SliderFloat("FOV", &renderInfo.cameraFieldOfView, 0.0f, 140.0f);

  static std::chrono::high_resolution_clock timer;
  static std::chrono::time_point<std::chrono::high_resolution_clock> prevFrame;
  auto curFrame = timer.now();

  auto delta =
    std::chrono::duration_cast<std::chrono::duration<float, std::micro>>
      (curFrame - prevFrame).count();

  ImGui::Text("%.2f ms / frame", delta / 1000.0f);

  prevFrame = curFrame;


  ImGui::End();
}
} // -- end anon namespace

CR_EXPORT int cr_main(struct cr_plugin * ctx, enum cr_op operation) {
  // return immediately if an update, this won't do anything
  if (operation == CR_STEP) { return 0; }

  if (!ctx) {
    spdlog::error("error loading base ui plugin, no context\n");
    return -1;
  }
  if (!ctx->userdata) {
    spdlog::error("error loading base ui plugin, no userdata\n");
    return -1;
  }

  auto & userInterface =
    *reinterpret_cast<mt::PluginInfoUserInterface*>(ctx->userdata);

  switch (operation) {
    case CR_LOAD:
      userInterface.Dispatch = &::Dispatch;
      userInterface.pluginType = mt::PluginType::UserInterface;
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  spdlog::info("base ui plugin successfully loaded");

  return 0;
}
