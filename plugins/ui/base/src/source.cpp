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
  if (!ImGui::Begin("Plugin Info")) { return; }

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

  std::array<float, Idx(mt::AspectRatio::size)> constexpr
    aspectRatioConversion = {{
      1.0f, 3.0f/2.0f, 4.0f/3.0f, 5.0f/4.0f, 16.0f/9.0f, 16.0f/10.0f, 21.0f/9.0f
    , 1.0f
    }};

  std::array<char const *, Idx(mt::AspectRatio::size)> constexpr
    aspectRatioLabels = {{ 
      "1x1", "3x2", "4x3", "5x4", "16x9", "16x10", "21x9", "None"
    }};

  for (size_t i = 0; i < pluginInfo.integrators.size(); ++ i) {
    auto & data       = renderInfo.integratorData[i];
    auto & integrator = pluginInfo.integrators[i];

    std::string label =
      std::string{integrator.pluginLabel} + std::string{" (config)"};

    ImGui::Begin(label.c_str());

    std::array<char const *, Idx(mt::RenderingState::Size)> stateStrings = {{
      "Off", "On Change", "After Change", "On Always"
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

    { // aspect ratio
      int value = static_cast<int>(data.imageAspectRatio);
      if (ImGui::BeginCombo("Aspect Ratio", aspectRatioLabels[value])) {
        for (size_t i = 0; i < aspectRatioLabels.size(); ++ i) {
          bool isSelected = value == static_cast<int>(i);
          if (ImGui::Selectable(aspectRatioLabels[i], isSelected)) {
            data.imageAspectRatio = static_cast<mt::AspectRatio>(value);

            // update image resolution
            data.imageResolution[1] =
              glm::max(
                1.0f
              , data.imageResolution[0] / aspectRatioConversion[value]
              );

            data.AllocateGlResources(renderInfo);
          }
        }
        ImGui::EndCombo();
      }
    }

    { // -- image resolution
      // to check which was changed
      auto previousImageResolution = data.imageResolution[0];
      if (ImGui::InputInt2("Image Resolution", &data.imageResolution.x)) {
        // only 4k support
        data.imageResolution =
          glm::clamp(data.imageResolution, glm::u16vec2(1), glm::u16vec2(4096));

        // apply aspect ratio change dependent on which item changed
        if (data.imageAspectRatio != mt::AspectRatio::eNone) {
          if (previousImageResolution != data.imageResolution[0]) {
            data.imageResolution[1] =
              glm::max(
                1.0f
              , data.imageResolution[0]
              / aspectRatioConversion[Idx(data.imageAspectRatio)]
              );
          } else {
            data.imageResolution[0] =
              data.imageResolution[1]
            * aspectRatioConversion[Idx(data.imageAspectRatio)];
          }
        }

        // must reallocate
        data.AllocateGlResources(renderInfo);
      }
    }

    ImGui::Text("%lu collected samples", data.collectedSamples);

    ImGui::End();
  }

  for (size_t i = 0; i < pluginInfo.integrators.size(); ++ i) {
    auto & data = renderInfo.integratorData[i];
    auto & integrator     = pluginInfo.integrators[i];

    std::string label =
      std::string{integrator.pluginLabel} + std::string{" (image)"};

    ImGui::Begin(label.c_str());

      ImGui::Image(
        reinterpret_cast<void*>(data.renderedTexture.handle)
      , ImVec2(data.imageResolution.x, data.imageResolution.y)
      );

      // clear out the image pixel clicked from previous frame
      data.imagePixelClicked = false;

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

        auto const pixel = (mousePos - itemMin);

        // store results, also have to tell render info which image was clicked
        data.imagePixelClickedCoord = glm::uvec2(glm::round(pixel));
        data.imagePixelClicked = true;
        renderInfo.lastIntegratorImageClicked = i;
      }

    ImGui::End();
  }
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
