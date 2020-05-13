#include <monte-toad/enum.hpp>
#include <monte-toad/scene.hpp>
#include <mt-plugin/plugin.hpp>

#include <cr/cr.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>
#include <imgui/imgui.hpp>
#include <spdlog/spdlog.h>

#include <chrono>

namespace {

float msTime = 0.0f;
float mouseSensitivity = 1.0f;

void UiCameraControls(
  mt::RenderInfo & renderInfo
) {
  /* if (ImGui::IsAnyItemActive()) { return; } */

  /* glm::vec3 dir = */ 

  // clamp to prevent odd ms time (like if scene were to load)
  msTime = glm::clamp(msTime, 0.0f, 155.5f);

  auto const cameraRight =
    glm::normalize(
      glm::cross(renderInfo.cameraDirection, renderInfo.cameraUpAxis)
    );
  auto const & cameraForward = renderInfo.cameraDirection;
  auto const & cameraUp = renderInfo.cameraUpAxis;

  auto window = reinterpret_cast<GLFWwindow *>(renderInfo.glfwWindow);

  if (glfwGetKey(window, GLFW_KEY_A)) {
    renderInfo.cameraOrigin -= msTime * cameraRight;
  }

  if (glfwGetKey(window, GLFW_KEY_D)) {
    renderInfo.cameraOrigin += msTime * cameraRight;
  }

  if (glfwGetKey(window, GLFW_KEY_W)) {
    renderInfo.cameraOrigin += msTime * cameraForward;
  }

  if (glfwGetKey(window, GLFW_KEY_S)) {
    renderInfo.cameraOrigin -= msTime * cameraForward;
  }

  if (glfwGetKey(window, GLFW_KEY_SPACE)) {
    renderInfo.cameraOrigin -= msTime * cameraUp;
  }

  if (
      glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)
   && glfwGetKey(window, GLFW_KEY_SPACE)
  ) {
    renderInfo.cameraOrigin += msTime * 2.0f * cameraUp;
  }

  static double prevX = -1.0, prevY = -1.0;
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {

    // hide as mouse will spaz around screen being teleported
    ImGui::SetMouseCursor(ImGuiMouseCursor_None);

    if (prevX == -1.0) {
      glfwGetCursorPos(window, &prevX, &prevY);
    }

    double deltaX, deltaY;
    glfwGetCursorPos(window, &deltaX, &deltaY);

    glm::vec2 delta = glm::vec2(deltaX - prevX, deltaY - prevY);

    renderInfo.cameraDirection +=
      (delta.x * cameraRight + delta.y * cameraUp)
    * ::msTime * 0.00005f * ::mouseSensitivity
    ;
    renderInfo.cameraDirection = glm::normalize(renderInfo.cameraDirection);

    if (std::isnan(renderInfo.cameraDirection.x)) {
      renderInfo.cameraDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glfwSetCursorPos(window, prevX, prevY);

  } else { prevX = -1.0; }
}

void UiSceneInfo(mt::Scene & scene) {
  static bool open = true;
  if (!ImGui::Begin("Scene Info", &open)) { return; }

  ImGui::Text(
    "   (%.2f, %.2f, %.2f)\n-> (%.2f, %.2f, %.2f) scene bounds"
  , scene.bboxMin.x, scene.bboxMin.y, scene.bboxMin.z
  , scene.bboxMax.x, scene.bboxMax.y, scene.bboxMax.z
  );

  auto textureString = fmt::format("{} textures", scene.textures.size());
  if (ImGui::TreeNode(textureString.c_str())) {
    for (auto const & texture : scene.textures) {
      ImGui::Text(
        "'%s' %lux%lu", texture.filename.c_str(), texture.width, texture.height
      );
    }
  }

  // TODO mesh display
  ImGui::Text("%lu meshes", scene.meshes.size());

  ImGui::End();
}

void UiPluginInfo(
  mt::Scene & scene
, mt::RenderInfo & renderInfo
, mt::PluginInfo & pluginInfo
, mt::DiagnosticInfo &
) {
  if (!(ImGui::Begin("Plugin Info"))) { return; }

  ImGui::Text("samples per pixel %lu", renderInfo.samplesPerPixel);
  ImGui::Text("paths per sample %lu", renderInfo.pathsPerSample);

  float
    min = glm::min(scene.bboxMin.x, glm::min(scene.bboxMin.y, scene.bboxMin.z))
  , max = glm::max(scene.bboxMax.x, glm::max(scene.bboxMax.y, scene.bboxMax.z));

  // allow viewing outside of min/max bounds
  min -= glm::abs(min)*1.5f;
  max += glm::abs(max)*1.5f;

  ImGui::SliderFloat3("Origin", &renderInfo.cameraOrigin.x, min, max);
  if (
    ImGui::SliderFloat3(
      "Direction", &renderInfo.cameraDirection.x, -1.0f, +1.0f
    )
  ) {
    renderInfo.cameraDirection = glm::normalize(renderInfo.cameraDirection);
  }
  ImGui::SliderFloat("Mouse Sensitivity", &::mouseSensitivity, 0.1f, 10.0f);
  ImGui::SliderFloat("FOV", &renderInfo.cameraFieldOfView, 0.0f, 140.0f);

  static std::chrono::high_resolution_clock timer;
  static std::chrono::time_point<std::chrono::high_resolution_clock> prevFrame;
  auto curFrame = timer.now();

  ::msTime =
    std::chrono::duration_cast<std::chrono::duration<float, std::micro>>
      (curFrame - prevFrame).count() / 1000.0f;

  ImGui::Text("%.2f ms / frame", ::msTime);

  prevFrame = curFrame;

  ImGui::End();
}

void UiImageOutput(
  mt::Scene & scene
, mt::RenderInfo & renderInfo
, mt::PluginInfo & pluginInfo
, mt::DiagnosticInfo & diagnosticInfo
) {
  if (!(ImGui::Begin("Image Output"))) { return; }

  static auto pixelInspectingCoord = glm::ivec2(-1);
  static auto pixelInspectingColor = glm::vec3();

  static std::array<int, 2> imGuiImageResolution = {{
    static_cast<int>(renderInfo.imageResolution[0])
  , static_cast<int>(renderInfo.imageResolution[1])
  }};
  static bool shareimGuiImageResolution = true;

  // display image
  ImGui::Image(
    diagnosticInfo.textureHandle
  , ImVec2(imGuiImageResolution[0], imGuiImageResolution[1])
  );

  // if image is clicked, approximate the clicked pixel, taking into account
  // image resolution differences when displaying to imgui
  if (ImGui::IsItemClicked()) {
    auto const
      imItemMin  = ImGui::GetItemRectMin()
    , imItemMax  = ImGui::GetItemRectMax()
    , imMousePos = ImGui::GetMousePos()
    ;

    auto const
      itemMin  = glm::vec2(imItemMin.x, imItemMin.y)
    , itemMax  = glm::vec2(imItemMax.x, imItemMax.y)
    , mousePos =
        glm::clamp(glm::vec2(imMousePos.x, imMousePos.y), itemMin, itemMax)
    ;

    auto const trueResolution =
      glm::vec2(renderInfo.imageResolution[0], renderInfo.imageResolution[1]);

    auto const resolutionRatio =
      trueResolution
    / glm::vec2(imGuiImageResolution[0], imGuiImageResolution[1]);

    auto const pixel = (mousePos - itemMin) * resolutionRatio;

    pixelInspectingCoord =
      glm::clamp(
        glm::ivec2(glm::round(pixel))
      , glm::ivec2(0)
      , glm::ivec2(trueResolution)
      );

    pixelInspectingColor =
      diagnosticInfo.currentFragmentBuffer[
        pixelInspectingCoord.y * renderInfo.imageResolution[0]
      + pixelInspectingCoord.x
      ];
  }

  // -- display resolution information
  ImGui::Checkbox("Sync imgui image resolution", &shareimGuiImageResolution);

  if (!shareimGuiImageResolution) {
    ImGui::InputInt2(
      "Texture display resolution"
    , imGuiImageResolution.data()
    );
  } else {
    imGuiImageResolution[0] = static_cast<int>(renderInfo.imageResolution[0]);
    imGuiImageResolution[1] = static_cast<int>(renderInfo.imageResolution[1]);
  }

  // -- display inspected pixel diagnostic information
  if (ImGui::ArrowButton("##arrow", ImGuiDir_Right)) {
    pixelInspectingCoord = glm::ivec2(-1);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100.0f);
  if (ImGui::InputInt2("Fragment Inspection", &pixelInspectingCoord.x)) {
    if (pixelInspectingCoord.x > 0 && pixelInspectingCoord.y > 0) {
      pixelInspectingColor =
        diagnosticInfo.currentFragmentBuffer[
          pixelInspectingCoord.y * renderInfo.imageResolution[0]
        + pixelInspectingCoord.x
        ];
    }
  }

  ImGui::SameLine();
  if (pixelInspectingCoord.x > 0 && pixelInspectingCoord.y > 0) {

    // just ensure that image resolution is correct
    pixelInspectingCoord.x =
      glm::min(
        static_cast<int>(renderInfo.imageResolution[0]), pixelInspectingCoord.x
      );
    pixelInspectingCoord.y =
      glm::min(
        static_cast<int>(renderInfo.imageResolution[1]), pixelInspectingCoord.y
      );

    ImGui::ColorButton(
      ""
    , ImVec4(
        pixelInspectingColor.x, pixelInspectingColor.y, pixelInspectingColor.z
      , 1.0f
      )
    );
  }

  ImGui::End();
}

void Dispatch(
  mt::Scene & scene
, mt::RenderInfo & renderInfo
, mt::PluginInfo & pluginInfo
, mt::DiagnosticInfo & diagnosticInfo
) {
  ::UiCameraControls(renderInfo);
  ::UiSceneInfo(scene);
  ::UiPluginInfo(scene, renderInfo, pluginInfo, diagnosticInfo);
  ::UiImageOutput(scene, renderInfo, pluginInfo, diagnosticInfo);
}

} // -- end anon namespace

CR_EXPORT int cr_main(struct cr_plugin * ctx, enum cr_op operation) {
  // return immediately if an update, this won't do anything
  if (operation == CR_STEP || operation == CR_UNLOAD) { return 0; }
  if (!ctx || !ctx->userdata) { return 0; }

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
