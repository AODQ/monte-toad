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

std::array<float, Idx(mt::AspectRatio::size)> constexpr
  aspectRatioConversion = {{
    1.0f, 3.0f/2.0f, 4.0f/3.0f, 5.0f/4.0f, 16.0f/9.0f, 16.0f/10.0f, 21.0f/9.0f
  , 1.0f
  }};

std::array<char const *, Idx(mt::AspectRatio::size)> constexpr
  aspectRatioLabels = {{
    "1x1", "3x2", "4x3", "5x4", "16x9", "16x10", "21x9", "None"
  }};

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
  if (!ImGui::Begin("Plugin Info")) { ImGui::End(); return; }

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

void ApplyImageResolutionConstraint(
  glm::u16vec2 & resolution, mt::AspectRatio aspectRatio
) {
  // must be a multiple of 8
  resolution -= (resolution % glm::u16vec2(8));

  // only 4k support
  resolution = glm::clamp(resolution, glm::u16vec2(8), glm::u16vec2(4096));

  // apply aspect ratio change dependent
  if (aspectRatio != mt::AspectRatio::eNone) {
    resolution.y =
      glm::max(
        1.0f, resolution.x / ::aspectRatioConversion[Idx(aspectRatio)]
      );
  }
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

          // don't clear out data if set to off
          if (data.renderingState != mt::RenderingState::Off)
            { mt::Clear(data); }
        }
      }
      ImGui::EndCombo();
    }

    if (ImGui::InputInt("samples per pixel", &data.samplesPerPixel)) {
      data.samplesPerPixel = glm::max(data.samplesPerPixel, 1ul);
      mt::Clear(data);
    }

    if (ImGui::InputInt("paths per sample", &data.pathsPerSample)) {
      data.pathsPerSample = glm::clamp(data.pathsPerSample, 1ul, 16ul);
      mt::Clear(data);
    }

    if (ImGui::InputInt("iterations per hunk", &data.blockInternalIteratorMax)){
      data.blockInternalIteratorMax =
        glm::clamp(data.blockInternalIteratorMax, 1ul, 64ul);
    }

    { // -- apply image resolution/aspect ratio fixes

      auto previousResolution = data.imageResolution;

      { // aspect ratio
        int value = static_cast<int>(data.imageAspectRatio);
        if (ImGui::BeginCombo("aspect ratio", ::aspectRatioLabels[value])) {
          for (size_t i = 0; i < ::aspectRatioLabels.size(); ++ i) {
            bool isSelected = value == static_cast<int>(i);
            if (ImGui::Selectable(::aspectRatioLabels[i], isSelected)) {
              data.imageAspectRatio = static_cast<mt::AspectRatio>(i);

              ApplyImageResolutionConstraint(
                data.imageResolution
              , data.imageAspectRatio
              );
            }
          }
          ImGui::EndCombo();
        }
      }

      // image resolution
      if (ImGui::InputInt("image resolution", &data.imageResolution.x, 8)) {
        ApplyImageResolutionConstraint(
          data.imageResolution
        , data.imageAspectRatio
        );
      }

      // -- imgui image resolution
      if (
        ImGui::Checkbox(
          "override imgui resolution"
        , &data.overrideImGuiImageResolution
        )
      ) {
        data.imguiImageResolution = data.imageResolution.x;
      }

      if (data.overrideImGuiImageResolution) {
        ImGui::InputInt("ImGui resolution", &data.imguiImageResolution);
      }

      // must reallocate resources if resolution has changed
      if (previousResolution != data.imageResolution)
        { mt::AllocateGlResources(data, renderInfo); }
    }

    if (data.renderingFinished) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "rendering completed");
    }

    ImGui::Text(
      "image resolution <%u, %u>"
    , data.imageResolution.x, data.imageResolution.y
    );

    if (data.overrideImGuiImageResolution) {
      ImGui::Text(
        "imgui resolution <%u, %u>"
      , data.imguiImageResolution
      , static_cast<uint32_t>(
          data.imguiImageResolution
        / ::aspectRatioConversion[Idx(data.imageAspectRatio)]
        )
      );
    }

    ImGui::Text("%lu dispatched cycles", data.dispatchedCycles);

    ImGui::Text(
      "%lu / %lu finished pixels"
    , mt::FinishedPixels(data), mt::FinishedPixelsGoal(data)
    );

    size_t finishedBlocks = 0;
    for (auto & i : data.blockPixelsFinished) {
      finishedBlocks +=
        static_cast<size_t>(
          i >= data.blockIteratorStride*data.blockIteratorStride
        );
    }
    ImGui::Text(
      "%lu / %lu finished blocks"
    , finishedBlocks
    , data.blockPixelsFinished.size()
    );
    ImGui::Text("%lu block iterator", data.blockIterator);

    ImGui::End();
  }

  for (size_t i = 0; i < pluginInfo.integrators.size(); ++ i) {
    auto & data = renderInfo.integratorData[i];
    auto & integrator = pluginInfo.integrators[i];

    std::string label =
      std::string{integrator.pluginLabel} + std::string{" (image)"};

    ImGui::Begin(label.c_str());

      // get image resolution, which might be overriden by user
      auto imageResolution = data.imageResolution;
      if (data.overrideImGuiImageResolution) {
        imageResolution.x = data.imguiImageResolution;
        imageResolution.y =
          imageResolution.x
        / ::aspectRatioConversion[Idx(data.imageAspectRatio)];
      }

      ImGui::Image(
        reinterpret_cast<void*>(data.renderedTexture.handle)
      , ImVec2(imageResolution.x, imageResolution.y)
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

        auto const resolutionRatio =
          glm::vec2(imageResolution) / glm::vec2(data.imageResolution);

        auto const pixel = (mousePos - itemMin) / resolutionRatio;

        // store results, also have to tell render info which image was clicked
        data.imagePixelClickedCoord = glm::uvec2(glm::round(pixel));
        data.imagePixelClicked = true;
        renderInfo.lastIntegratorImageClicked = i;
      }

    ImGui::End();
  }
}

void UiEmitters(
  mt::Scene & scene
, mt::RenderInfo & renderInfo
, mt::PluginInfo const & pluginInfo
) {
  ImGui::Begin("emitters");
    { // select skybox emitter
      auto GetEmissionLabel = [&](int32_t idx) -> char const * {
        return idx == -1 ? "none" : pluginInfo.emitters[idx].pluginLabel;
      };

      int32_t & emissionIdx = scene.emissionSource.skyboxEmitterPluginIdx;
      if (ImGui::BeginCombo("Skybox", GetEmissionLabel(emissionIdx))) {
        for (size_t i = 0; i < pluginInfo.emitters.size(); ++ i) {
          if (!pluginInfo.emitters[i].isSkybox) { continue; }
          bool isSelected = emissionIdx == static_cast<int32_t>(i);
          if (ImGui::Selectable(GetEmissionLabel(i), isSelected)) {
            emissionIdx = static_cast<int32_t>(i);
            renderInfo.ClearImageBuffers();
          }
        }
        ImGui::EndCombo();
      }
    }
  ImGui::End();
}

void Dispatch(
  mt::Scene & scene
, mt::RenderInfo & renderInfo
, mt::PluginInfo const & pluginInfo
) {
  ::UiCameraControls(scene, renderInfo);
  ::UiPluginInfo(scene, renderInfo, pluginInfo);
  ::UiImageOutput(scene, renderInfo, pluginInfo);
  ::UiEmitters(scene, renderInfo, pluginInfo);
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

  return 0;
}
