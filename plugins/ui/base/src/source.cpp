#include <cr/cr.h>

#include <monte-toad/enum.hpp>
#include <monte-toad/imgui.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/scene.hpp>
#include <mt-plugin/plugin.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <chrono>

namespace {

float msTime = 0.0f;
float CR_STATE mouseSensitivity = 1.0f;
float CR_STATE cameraRelativeVelocity = 1.0f;

void UiCameraControls(mt::Scene const & scene, mt::RenderInfo & renderInfo) {
  // clamp to prevent odd ms time (like if scene were to load)
  msTime = glm::clamp(msTime, 1.0f, 10055.5f);

  auto const cameraRight =
    glm::normalize(
      glm::cross(renderInfo.cameraDirection, renderInfo.cameraUpAxis)
    );
  auto const & cameraForward = renderInfo.cameraDirection;
  auto const & cameraUp = renderInfo.cameraUpAxis;

  auto const cameraVelocity =
    glm::length(scene.bboxMax - scene.bboxMin)
  * 0.001f * cameraRelativeVelocity;

  auto window = reinterpret_cast<GLFWwindow *>(renderInfo.glfwWindow);

  bool cameraHasMoved = false;

  if (glfwGetKey(window, GLFW_KEY_A)) {
    renderInfo.cameraOrigin -= msTime * cameraRight * cameraVelocity;
    cameraHasMoved = true;
  }

  if (glfwGetKey(window, GLFW_KEY_D)) {
    renderInfo.cameraOrigin += msTime * cameraRight * cameraVelocity;
    cameraHasMoved = true;
  }

  if (glfwGetKey(window, GLFW_KEY_W)) {
    renderInfo.cameraOrigin += msTime * cameraForward * cameraVelocity;
    cameraHasMoved = true;
  }

  if (glfwGetKey(window, GLFW_KEY_S)) {
    renderInfo.cameraOrigin -= msTime * cameraForward * cameraVelocity;
    cameraHasMoved = true;
  }

  if (glfwGetKey(window, GLFW_KEY_SPACE)) {
    renderInfo.cameraOrigin -= msTime * cameraUp * cameraVelocity;
    cameraHasMoved = true;
  }

  if (
      glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)
   && glfwGetKey(window, GLFW_KEY_SPACE)
  ) {
    renderInfo.cameraOrigin += msTime * 2.0f * cameraUp * cameraVelocity;
    cameraHasMoved = true;
  }

  static CR_STATE double prevX = -1.0, prevY = -1.0;
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
    * ::msTime * 0.00025f * ::mouseSensitivity
    ;
    renderInfo.cameraDirection = glm::normalize(renderInfo.cameraDirection);

    if (std::isnan(renderInfo.cameraDirection.x)) {
      renderInfo.cameraDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glfwSetCursorPos(window, prevX, prevY);

    cameraHasMoved = true;

  } else { prevX = -1.0; }

  if (cameraHasMoved) { renderInfo.ClearImageBuffers(); }
}

void UiPluginInfo(
  mt::Scene & scene
, mt::RenderInfo & renderInfo
, mt::PluginInfo const & pluginInfo
) {
  if (!(ImGui::Begin("Plugin Info"))) { return; }

  // TODO
  /* ImGui::Text("samples per pixel %lu", renderInfo.samplesPerPixel); */

  /* { */
  /*   int pps = static_cast<int>(renderInfo.pathsPerSample); */
  /*   ImGui::InputInt("paths per sample", &pps); */
  /*   pps = glm::clamp(pps, 0, 64); */
  /*   renderInfo.pathsPerSample = static_cast<size_t>(pps); */
  /* } */

  float
    min = glm::min(scene.bboxMin.x, glm::min(scene.bboxMin.y, scene.bboxMin.z))
  , max = glm::max(scene.bboxMax.x, glm::max(scene.bboxMax.y, scene.bboxMax.z));

  // allow viewing outside of min/max bounds
  min -= glm::abs(min)*1.5f;
  max += glm::abs(max)*1.5f;

  if (ImGui::SliderFloat3("Origin", &renderInfo.cameraOrigin.x, min, max)) {
    renderInfo.ClearImageBuffers();
  }

  if (ImGui::InputFloat3("Camera up axis", &renderInfo.cameraUpAxis.x)) {
    renderInfo.ClearImageBuffers();
  }
  if (ImGui::Button("Normalize camera up")) {
    renderInfo.cameraUpAxis = glm::normalize(renderInfo.cameraUpAxis);
    renderInfo.ClearImageBuffers();
  }

  if (
    ImGui::SliderFloat3(
      "Direction", &renderInfo.cameraDirection.x, -1.0f, +1.0f
    )
  ) {
    renderInfo.cameraDirection = glm::normalize(renderInfo.cameraDirection);
    renderInfo.ClearImageBuffers();
  }
  ImGui::SliderFloat("Mouse Sensitivity", &::mouseSensitivity, 0.1f, 3.0f);
  ImGui::SliderFloat("Camera Velocity", &::cameraRelativeVelocity, 0.1f, 2.0f);
  if (ImGui::SliderFloat("FOV", &renderInfo.cameraFieldOfView, 0.0f, 140.0f)) {
    renderInfo.ClearImageBuffers();
  }

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
, mt::PluginInfo const & pluginInfo
) {

  for (size_t i = 0; i < pluginInfo.integrators.size(); ++ i) {
    auto & data       = renderInfo.integratorData[i];
    auto & integrator = pluginInfo.integrators[i];

    std::string label =
      std::string{integrator.pluginLabel} + std::string{" (config)"};

    if (!ImGui::Begin(label.c_str()))
      { continue; }

    std::array<char const *, Idx(mt::RenderingState::Size)> stateStrings = {{
      "Off", "On Change", "On Always"
    }};

    int value = static_cast<int>(data.renderingState);

    if (ImGui::BeginCombo("State", stateStrings[value])) {
      for (size_t i = 0; i < stateStrings.size(); ++ i) {
        bool isSelected = value == static_cast<int>(i);
        if (ImGui::Selectable(stateStrings[i], isSelected)) {
          data.renderingState = static_cast<mt::RenderingState>(i);
          data.Clear();
        }
      }
      ImGui::EndCombo();
    }

    if (ImGui::InputInt("Samples per pixel", &data.samplesPerPixel)) {
      data.samplesPerPixel = glm::max(data.samplesPerPixel, 1ul);
    }

    if (ImGui::InputInt("Paths per sample", &data.pathsPerSample)) {
      data.pathsPerSample = glm::clamp(data.pathsPerSample, 1ul, 16ul);
    }

    ImGui::Text("%lu collected samples", data.collectedSamples);

    ImGui::End();
  }

  for (size_t i = 0; i < pluginInfo.integrators.size(); ++ i) {
    auto & integratorData = renderInfo.integratorData[i];
    auto & integrator     = pluginInfo.integrators[i];

    std::string label =
      std::string{integrator.pluginLabel} + std::string{" (image)"};

    if (!ImGui::Begin(label.c_str()))
      { continue; }

    ImGui::Image(
      reinterpret_cast<void*>(integratorData.renderedTexture.handle)
    , ImVec2(integratorData.imageResolution.x, integratorData.imageResolution.y)
    );

    ImGui::End();
  }


  // TODO
  /* if (!(ImGui::Begin("Image Output"))) { return; } */

  /* static CR_STATE auto pixelInspectingCoord = glm::ivec2(-1); */
  /* static CR_STATE auto pixelInspectingColor = glm::vec3(); */

  /* static CR_STATE std::array<int, 2> imGuiImageResolution = {{ */
  /*   static_cast<int>(renderInfo.imageResolution[0]) */
  /* , static_cast<int>(renderInfo.imageResolution[1]) */
  /* }}; */
  /* static CR_STATE bool shareimGuiImageResolution = true; */

  /* // -- display resolution information */
  /* ImGui::Checkbox("Sync imgui image resolution", &shareimGuiImageResolution); */

  /* if (!shareimGuiImageResolution) { */
  /*   ImGui::InputInt( */
  /*     "Texture display resolution" */
  /*   , imGuiImageResolution.data() */
  /*   ); */
  /*   imGuiImageResolution[1] = */
  /*     imGuiImageResolution[0] */
  /*   * ( */
  /*       renderInfo.imageResolution[1] */
  /*     / static_cast<float>(renderInfo.imageResolution[0]) */
  /*     ); */
  /* } else { */
  /*   imGuiImageResolution[0] = static_cast<int>(renderInfo.imageResolution[0]); */
  /*   imGuiImageResolution[1] = static_cast<int>(renderInfo.imageResolution[1]); */
  /* } */

  /* // -- display inspected pixel diagnostic information */
  /* if (ImGui::ArrowButton("##arrow", ImGuiDir_Right)) { */
  /*   pixelInspectingCoord = glm::ivec2(-1); */
  /* } */
  /* ImGui::SameLine(); */
  /* ImGui::SetNextItemWidth(100.0f); */
  /* if (ImGui::InputInt2("Fragment Inspection", &pixelInspectingCoord.x)) { */
  /*   if (pixelInspectingCoord.x > 0 && pixelInspectingCoord.y > 0) { */
  /*     pixelInspectingColor = */
  /*       diagnosticInfo.currentFragmentBuffer[ */
  /*         pixelInspectingCoord.y * renderInfo.imageResolution[0] */
  /*       + pixelInspectingCoord.x */
  /*       ]; */
  /*   } */
  /* } */

  /* if (pixelInspectingCoord.x > 0 && pixelInspectingCoord.y > 0) { */
  /*   // just ensure that image resolution is correct */
  /*   pixelInspectingCoord.x = */
  /*     glm::min( */
  /*       static_cast<int>(renderInfo.imageResolution[0]), pixelInspectingCoord.x */
  /*     ); */
  /*   pixelInspectingCoord.y = */
  /*     glm::min( */
  /*       static_cast<int>(renderInfo.imageResolution[1]), pixelInspectingCoord.y */
  /*     ); */

  /*   ImGui::SameLine(); */
  /*   ImGui::ColorButton( */
  /*     "" */
  /*   , ImVec4( */
  /*       pixelInspectingColor.x, pixelInspectingColor.y, pixelInspectingColor.z */
  /*     , 1.0f */
  /*     ) */
  /*   ); */
  /* } */

  /* // display image */
  /* ImGui::Image( */
  /*   diagnosticInfo.textureHandle */
  /* , ImVec2(imGuiImageResolution[0], imGuiImageResolution[1]) */
  /* ); */

  /* // clear out the image pixel clicked from previous frame */
  /* renderInfo.imagePixelClicked = false; */

  /* // if image is clicked, approximate the clicked pixel, taking into account */
  /* // image resolution differences when displaying to imgui */
  /* if (ImGui::IsItemClicked()) { */
  /*   auto const */
  /*     imItemMin  = ImGui::GetItemRectMin() */
  /*   , imItemMax  = ImGui::GetItemRectMax() */
  /*   , imMousePos = ImGui::GetMousePos() */
  /*   ; */

  /*   auto const */
  /*     itemMin  = glm::vec2(imItemMin.x, imItemMin.y) */
  /*   , itemMax  = glm::vec2(imItemMax.x, imItemMax.y) */
  /*   , mousePos = */
  /*       glm::clamp(glm::vec2(imMousePos.x, imMousePos.y), itemMin, itemMax) */
  /*   ; */

  /*   auto const trueResolution = */
  /*     glm::vec2(renderInfo.imageResolution[0], renderInfo.imageResolution[1]); */

  /*   auto const resolutionRatio = */
  /*     trueResolution */
  /*   / glm::vec2(imGuiImageResolution[0], imGuiImageResolution[1]); */

  /*   auto const pixel = (mousePos - itemMin) * resolutionRatio; */

  /*   pixelInspectingCoord = */
  /*     glm::clamp( */
  /*       glm::ivec2(glm::round(pixel)) */
  /*     , glm::ivec2(0) */
  /*     , glm::ivec2(trueResolution) */
  /*     ); */

  /*   pixelInspectingColor = */
  /*     diagnosticInfo.currentFragmentBuffer[ */
  /*       pixelInspectingCoord.y * renderInfo.imageResolution[0] */
  /*     + pixelInspectingCoord.x */
  /*     ]; */

  /*   renderInfo.imagePixel = glm::uvec2(glm::round(pixel)); */
  /*   renderInfo.imagePixelClicked = true; */
  /* } */

  /* ImGui::End(); */
}

void Dispatch(
  mt::Scene & scene
, mt::RenderInfo & renderInfo
, mt::PluginInfo const & pluginInfo
) {
  ::UiCameraControls(scene, renderInfo);
  ::UiPluginInfo(scene, renderInfo, pluginInfo);
  ::UiImageOutput(scene, renderInfo, pluginInfo);
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
      userInterface.pluginLabel = "base user interface";
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  spdlog::info("base ui plugin successfully loaded");

  return 0;
}
