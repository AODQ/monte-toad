// base ui

#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/kerneldispatchinfo.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <monte-toad/core/texture.hpp>
#include <monte-toad/core/triangle.hpp>
#include <monte-toad/util/file.hpp>
#include <monte-toad/util/textureloader.hpp>
#include <mt-plugin/plugin.hpp>

#include <GLFW/glfw3.h>
#include <imgui/imgui.hpp>
#include <spdlog/spdlog.h>

#include <chrono>

namespace ImGui {
  bool InputInt(const char * label, size_t * value, int step = 1) {
    int valueAsInt = static_cast<int>(*value);
    bool result = ImGui::InputInt(label, &valueAsInt, step);
    *value = static_cast<size_t>(valueAsInt);
    return result;
  }

  bool InputInt(const char * label, uint16_t * value, int step = 1) {
    int valueAsInt = static_cast<int>(*value);
    bool result = ImGui::InputInt(label, &valueAsInt, step);
    *value = static_cast<uint16_t>(valueAsInt);
    return result;
  }

  bool InputInt2(const char * label, uint16_t * value, int step = 1) {
    int values[2] = { static_cast<int>(value[0]), static_cast<int>(value[1]) };
    bool result = ImGui::InputInt2(label, values, step);
    if (result) {
      value[0] = static_cast<uint16_t>(values[0]);
      value[1] = static_cast<uint16_t>(values[1]);
    }
    return result;
  }
}

namespace {

float msTime = 0.0f;
float mouseSensitivity = 1.0f;
float cameraRelativeVelocity = 1.0f;

std::array<char const *, Idx(mt::AspectRatio::size)> constexpr
  aspectRatioLabels = {{
    "1x1", "3x2", "4x3", "5x4", "16x9", "16x10", "21x9", "None"
  }};

std::array<size_t, 4> constexpr blockIteratorStrides = {{
  32ul, 64ul, 128ul, 256ul
}};

void UiCameraControls(
  mt::core::Scene const & scene
, mt::PluginInfo const & plugin
, mt::core::RenderInfo & render
) {

  static double prevX = -1.0, prevY = -1.0;

  auto window = reinterpret_cast<GLFWwindow *>(render.glfwWindow);

  // toggle rendering state
  static bool qPressed = false;
  if (glfwGetKey(window, GLFW_KEY_Q)) {
    render.globalRendering =
      qPressed ? render.globalRendering : !render.globalRendering;
    qPressed = true;
  } else { qPressed = false; }

  // only get to control camera by right clicking
  if (!glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
    prevX = -1.0;
    return;
  }

  // clamp to prevent odd ms time (like if scene were to load)
  ::msTime = glm::clamp(::msTime, 0.01f, 15.5f);

  auto const cameraRight =
    glm::normalize(
      glm::cross(render.camera.direction, render.camera.upAxis)
    );
  auto const & cameraForward = render.camera.direction;
  auto const & cameraUp = render.camera.upAxis;

  auto const cameraVelocity =
    glm::length(scene.bboxMax - scene.bboxMin)
  * 0.001f * cameraRelativeVelocity * ::msTime;

  if (glfwGetKey(window, GLFW_KEY_A)) {
    render.camera.origin += cameraRight * cameraVelocity;
  }

  if (glfwGetKey(window, GLFW_KEY_D)) {
    render.camera.origin -= cameraRight * cameraVelocity;
  }

  if (glfwGetKey(window, GLFW_KEY_W)) {
    render.camera.origin += cameraForward * cameraVelocity;
  }

  if (glfwGetKey(window, GLFW_KEY_S)) {
    render.camera.origin -= cameraForward * cameraVelocity;
  }

  if (glfwGetKey(window, GLFW_KEY_SPACE)) {
    render.camera.origin -= cameraUp * cameraVelocity;
  }

  if (
      glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)
   && glfwGetKey(window, GLFW_KEY_SPACE)
  ) {
    render.camera.origin += 2.0f * cameraUp * cameraVelocity;
  }

  // hide as mouse will spaz around screen being teleported
  ImGui::SetMouseCursor(ImGuiMouseCursor_None);

  if (prevX == -1.0) {
    glfwGetCursorPos(window, &prevX, &prevY);
  }

  double deltaX, deltaY;
  glfwGetCursorPos(window, &deltaX, &deltaY);

  glm::vec2 delta = glm::vec2(deltaX - prevX, deltaY - prevY);

  render.camera.direction +=
    (-delta.x * cameraRight + delta.y * cameraUp)
  * 0.00025f * ::mouseSensitivity * ::msTime
  ;
  render.camera.direction = glm::normalize(render.camera.direction);

  if (std::isnan(render.camera.direction.x)) {
    render.camera.direction = glm::vec3(1.0f, 0.0f, 0.0f);
  }

  glfwSetCursorPos(window, prevX, prevY);

  mt::core::UpdateCamera(plugin, render);
}

void UiPluginInfo(
  mt::core::Scene & scene
, mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  ImGui::Begin("Plugin Info");

  float
    min = glm::min(scene.bboxMin.x, glm::min(scene.bboxMin.y, scene.bboxMin.z))
  , max = glm::max(scene.bboxMax.x, glm::max(scene.bboxMax.y, scene.bboxMax.z));

  // allow viewing outside of min/max bounds
  min -= glm::abs(min)*1.5f;
  max += glm::abs(max)*1.5f;

  if (ImGui::SliderFloat3("Origin", &render.camera.origin.x, min, max)) {
    mt::core::UpdateCamera(plugin, render);
  }
  if (ImGui::Button("Clear Origin")) {
    render.camera.origin = glm::vec3(0.0f);
    mt::core::UpdateCamera(plugin, render);
  }

  if (ImGui::InputFloat3("Camera up axis", &render.camera.upAxis.x)) {
    mt::core::UpdateCamera(plugin, render);
  }
  if (ImGui::Button("Normalize camera up")) {
    render.camera.upAxis = glm::normalize(render.camera.upAxis);
    mt::core::UpdateCamera(plugin, render);
  }

  if (
    ImGui::SliderFloat3(
      "Direction", &render.camera.direction.x, -1.0f, +1.0f
    )
  ) {
    render.camera.direction = glm::normalize(render.camera.direction);
    mt::core::UpdateCamera(plugin, render);
  }
  ImGui::SliderFloat("Mouse Sensitivity", &::mouseSensitivity, 0.1f, 3.0f);
  ImGui::SliderFloat("Camera Velocity", &::cameraRelativeVelocity, 0.1f, 2.0f);
  if (ImGui::SliderFloat("FOV", &render.camera.fieldOfView, 0.0f, 140.0f)) {
    mt::core::UpdateCamera(plugin, render);
  }

  ImGui::Checkbox("rendering", &render.globalRendering);

  static std::chrono::high_resolution_clock timer;
  static std::chrono::time_point<std::chrono::high_resolution_clock> prevFrame;
  auto curFrame = timer.now();

  ::msTime =
    std::chrono::duration_cast<std::chrono::duration<float, std::micro>>
      (curFrame - prevFrame).count() / 1000.0f;

  ImGui::Text("%s", fmt::format("{:.2f} ms / frame", ::msTime).c_str());

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
  mt::ApplyAspectRatioY(aspectRatio, resolution.x, resolution.y);
}

void UiTextureEditor(mt::core::Scene & scene) {
  ImGui::Begin("Textures");

  if (ImGui::Button("Load Texture")) {
    auto files =
      mt::util::FilePickerMultiple(
          " --file-filter=\"image files | "
          " *.jpeg *.jpg *.png *.tga *.bmp *.psd *.gif *.hdr *.pic *.ppm"
          " *.pgm\""
          );
    for (auto const & filename : files) {
      auto texture = mt::util::LoadTexture(filename);
      if (texture.Valid()) {
        texture.label = filename;
        scene.textures.emplace_back(texture);
      } else {
        spdlog::error("Could not load texture '{}'", filename);
      }
    }
  }

  for (size_t texIt = 0; texIt != scene.textures.size(); ++ texIt) {
    // delete texture if requested
    if (ImGui::Button(fmt::format("X##{}", texIt).c_str())) {
      scene.textures.erase(scene.textures.begin() + texIt);
      -- texIt;
      continue;
    }
    ImGui::SameLine();
    ImGui::Text("%s##%lu", scene.textures[texIt].label.c_str(), texIt);
  }

  ImGui::End();
}

void UiKernelDispatchEditor(
  mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  static size_t integratorIdx = -1ul;

  if (render.lastIntegratorImageClicked != -1ul) {
    integratorIdx = render.lastIntegratorImageClicked;
  }

  // clear integrator if size has changed
  if (integratorIdx >= plugin.integrators.size()) {
    integratorIdx = -1ul;
  }

  ImGui::Begin("kernel dispatch editor");

  if (integratorIdx == -1ul) {
    ImGui::End();
    return;
  }

  auto & data = render.integratorData[integratorIdx];
  auto & integrator = plugin.integrators[integratorIdx];

  ImGui::Text("%s", integrator.PluginLabel());

  for (size_t idx = 0ul; idx < data.kernelDispatchers.size(); ++ idx) {
    auto & kernelDispatch = data.kernelDispatchers[idx];
    ImGui::PushID(idx);

    ImGui::NewLine();
    ImGui::Separator();
    ImGui::Separator();
    ImGui::NewLine();

    ImGui::Text(
      "%s", plugin.kernels[kernelDispatch.dispatchPluginIdx].PluginLabel()
    );

    if (ImGui::BeginCombo("Timing", mt::ToString(kernelDispatch.timing))) {
      for (size_t i = 0ul; i < Idx(mt::KernelDispatchTiming::Size); ++ i) {
        if (
          ImGui::Selectable(
            mt::ToString(static_cast<mt::KernelDispatchTiming>(i))
          , i == Idx(kernelDispatch.timing)
          )
        ) {
          kernelDispatch.timing = static_cast<mt::KernelDispatchTiming>(i);
        }
      }
      ImGui::EndCombo();
    }

    if (ImGui::Button("delete")) {
      data.kernelDispatchers.erase(data.kernelDispatchers.begin() + idx);
      -- idx;
    }

    // -- repositioning of kernels

    ImGui::SameLine();
    if (idx > 0ul && ImGui::Button("-")) {
      std::swap(data.kernelDispatchers[idx], data.kernelDispatchers[idx-1]);
    }

    ImGui::SameLine();
    if (idx < data.kernelDispatchers.size()-1 && ImGui::Button("+")) {
      std::swap(data.kernelDispatchers[idx], data.kernelDispatchers[idx+1]);
    }

    ImGui::PopID();
  }

  ImGui::NewLine();
  ImGui::NewLine();
  ImGui::Separator();
  ImGui::Separator();
  ImGui::NewLine();

  if (ImGui::BeginCombo("## kernel select", "add kernel")) {
    for (size_t i = 0ul; i < plugin.kernels.size(); ++ i) {
      if (ImGui::Selectable(plugin.kernels[i].PluginLabel(), false)) {
        mt::core::KernelDispatchInfo kernel;
        kernel.timing = mt::KernelDispatchTiming::Off;
        kernel.dispatchPluginIdx = i;
        // todo allocate kernel
        /* kernel.userdata */

        data.kernelDispatchers.emplace_back(std::move(kernel));
      }
    }

    ImGui::EndCombo();
  }

  ImGui::End();
}

void UiImageOutput(
    mt::core::Scene & /*scene*/
    , mt::core::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) {

  // -- display standard integrator options
  for (size_t i = 0; i < plugin.integrators.size(); ++ i) {
    auto & data       = render.integratorData[i];
    auto & integrator = plugin.integrators[i];

    std::string label =
      std::string{integrator.PluginLabel()} + std::string{" (config)"};

    ImGui::Begin(label.c_str());

    std::array<char const *, Idx(mt::RenderingState::Size)> stateStrings = {{
      "Off", "On Change", "After Change", "On Always"
    }};

    if (
        size_t value = static_cast<size_t>(data.renderingState);
        ImGui::BeginCombo("State", stateStrings[value])
       ) {
      for (size_t stateIt = 0; stateIt < stateStrings.size(); ++ stateIt) {
        bool isSelected = value == stateIt;
        if (ImGui::Selectable(stateStrings[stateIt], isSelected)) {
          data.renderingState = static_cast<mt::RenderingState>(stateIt);

          // don't clear out data if set to off
          if (data.renderingState != mt::RenderingState::Off)
          { mt::core::Clear(data); }
        }
      }
      ImGui::EndCombo();
    }

    // only display offline parameters if the integrator is not realtime
    if (!integrator.RealTime()) {
      if (ImGui::InputInt("samples per pixel", &data.samplesPerPixel)) {
        data.samplesPerPixel = glm::max(data.samplesPerPixel, 1ul);

        // don't clear data, but in case rendering is already complete allow it
        // to process again
        data.renderingFinished = false;
        for (auto & blockIt : data.blockPixelsFinished)
          { blockIt = 0; }
        data.unfinishedPixelsCount = 0u;
      }

      if (ImGui::InputInt("paths per sample", &data.pathsPerSample)) {
        data.pathsPerSample = glm::clamp(data.pathsPerSample, 1ul, 16ul);
        mt::core::Clear(data);
      }

      if (ImGui::InputInt("iterations per hunk", &data.blockInternalIteratorMax)){
        data.blockInternalIteratorMax =
          glm::clamp(data.blockInternalIteratorMax, 1ul, 64ul);
      }

      { // -- iterator block size
        size_t iteratorIdx = 0ul;
        // get current idx
        for (size_t idx = 0; idx < ::blockIteratorStrides.size(); ++ idx)
        {
          if (::blockIteratorStrides[idx] == data.blockIteratorStride) {
            iteratorIdx = idx;
            break;
          }
        }

        if (
            ImGui::BeginCombo(
              "block size"
              , std::to_string(::blockIteratorStrides[iteratorIdx]).c_str()
              )
           ) {
          for (
              size_t blockIt = 0;
              blockIt < ::blockIteratorStrides.size();
              ++ blockIt
              ) {
            bool isSelected = iteratorIdx == blockIt;
            if (
                ImGui::Selectable(
                  std::to_string(::blockIteratorStrides[blockIt]).c_str()
                  , isSelected)
               ) {
              data.blockIteratorStride = ::blockIteratorStrides[blockIt];
              mt::core::Clear(data);
            }
          }
          ImGui::EndCombo();
        }
      }
    }

    { // -- apply image resolution/aspect ratio fixes

      auto previousResolution = data.imageResolution;

      { // aspect ratio
        size_t value = static_cast<size_t>(data.imageAspectRatio);
        if (ImGui::BeginCombo("aspect ratio", ::aspectRatioLabels[value])) {
          for (size_t arIt = 0; arIt < ::aspectRatioLabels.size(); ++ arIt) {
            bool isSelected = value == arIt;
            if (ImGui::Selectable(::aspectRatioLabels[arIt], isSelected)) {
              data.imageAspectRatio = static_cast<mt::AspectRatio>(arIt);

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
      { mt::core::AllocateResources(data); }
    }

    if (data.renderingFinished) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "rendering completed");
    }

    ImGui::Text(
        "image resolution <%u, %u>"
        , data.imageResolution.x, data.imageResolution.y
        );

    if (data.overrideImGuiImageResolution) {
      uint16_t y;
      mt::ApplyAspectRatioY(
          data.imageAspectRatio, data.imguiImageResolution, y
          );

      ImGui::Text(
          "imgui resolution <%u, %u>"
          , data.imguiImageResolution
          , static_cast<uint32_t>(y)
          );
    }

    if (!integrator.RealTime()) {
      ImGui::Text("%lu dispatched cycles", data.dispatchedCycles);

      ImGui::Text(
          "%lu / %lu finished pixels"
          , mt::core::FinishedPixels(data), mt::core::FinishedPixelsGoal(data)
          );

      size_t finishedBlocks = 0;
      for (auto & blockIt : data.blockPixelsFinished) {
        finishedBlocks +=
          static_cast<size_t>(
              blockIt >= data.blockIteratorStride*data.blockIteratorStride
              );
      }
      ImGui::Text(
          "%lu / %lu finished blocks"
          , finishedBlocks
          , data.blockPixelsFinished.size()
          );
      ImGui::Text("%lu block iterator", data.blockIterator);
    }

    ImGui::End();
  }

  for (size_t i = 0; i < plugin.integrators.size(); ++ i) {
    auto & data = render.integratorData[i];
    auto & integrator = plugin.integrators[i];

    // get image resolution, which might be overriden by user
    auto imageResolution = data.imageResolution;
    if (data.overrideImGuiImageResolution) {
      imageResolution.x = data.imguiImageResolution;
      mt::ApplyAspectRatioY(
        data.imageAspectRatio, imageResolution.x, imageResolution.y
      );
    }

    if (!integrator.RealTime()) {
      std::string const label =
        std::string{integrator.PluginLabel()} + std::string{" (image preview)"};

      auto handle = data.previewRenderedTexture.handle;

      ImGui::Begin(label.c_str());
      ImGui::Image(
        reinterpret_cast<void*>(handle)
      , ImVec2(imageResolution.x, imageResolution.y)
      );
      ImGui::End();
    }

    std::string label =
      std::string{integrator.PluginLabel()} + std::string{" (image)"};

    ImGui::Begin(label.c_str());

    auto handle = data.renderedTexture.handle;

    ImGui::Image(
      reinterpret_cast<void*>(handle)
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

      auto pixel = (mousePos - itemMin) / resolutionRatio;

      // flip X axis of pixel
      pixel.x = data.imageResolution.x - pixel.x;

      // store results, also have to tell render info which image was clicked
      data.imagePixelClickedCoord = glm::uvec2(glm::round(pixel));
      data.imagePixelClicked = true;
      render.lastIntegratorImageClicked = i;
    }

    ImGui::End();
  }
}

void UiDispatchers(
  mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  ImGui::Begin("dispatchers");

  if (plugin.dispatchers.size() == 0) {
    ImGui::Text("No dispatcher plugin");
    ImGui::End();
    return;
  }

  auto const IntegratorLabel = [&](size_t idx) -> char const * {
    return idx == -1lu ? "N/A" : plugin.integrators[idx].PluginLabel();
  };

  for (size_t it = 0; it < Idx(mt::IntegratorTypeHint::Size); ++ it) {
    auto & idx = render.integratorIndices[it];

    auto comboStr =
      fmt::format(
          "Integrator {}"
          , mt::ToString(static_cast<mt::IntegratorTypeHint>(it))
          );
    if (!ImGui::BeginCombo(comboStr.c_str(), IntegratorLabel(idx)))
    { continue; }

    if (ImGui::Selectable("None", idx == -1lu)) {
      idx = -1lu;
    }

    for (size_t i = 0; i < plugin.integrators.size(); ++ i) {
      bool isSelected = idx == i;
      if (ImGui::Selectable(IntegratorLabel(i), isSelected)) { idx = i; }
    }
    ImGui::EndCombo();
  }

  auto const DispatcherLabel = [&](size_t idx) -> char const * {
    return plugin.dispatchers[idx].PluginLabel();
  };

  { // select a primary dispatcher
    auto & idx = render.primaryDispatcher;
    if (ImGui::BeginCombo("Dispatcher", DispatcherLabel(idx))) {
      for (size_t i = 0; i < plugin.dispatchers.size(); ++ i) {
        bool isSelected = idx == i;
        if (ImGui::Selectable(DispatcherLabel(i), isSelected)) {
          idx = i;
          render.ClearImageBuffers();
        }
      }
      ImGui::EndCombo();
    }
  }

  ImGui::End();
}

void UiEmitters(
    mt::core::Scene & scene
    , mt::core::RenderInfo & render
    , mt::PluginInfo const & plugin
    ) {
  ImGui::Begin("emitters");
  { // -- select skybox emitter
    auto GetEmissionLabel = [&](size_t idx) -> char const * {
      return idx == -1lu ? "none" : plugin.emitters[idx].PluginLabel();
    };

    size_t & emissionIdx = scene.emissionSource.skyboxEmitterPluginIdx;
    if (ImGui::BeginCombo("Skybox", GetEmissionLabel(emissionIdx))) {
      if (ImGui::Selectable("none", emissionIdx == -1lu)) {
        emissionIdx = -1lu;
        render.ClearImageBuffers();
      }
      for (size_t i = 0; i < plugin.emitters.size(); ++ i) {
        if (!plugin.emitters[i].IsSkybox()) { continue; }
        bool isSelected = emissionIdx == i;
        if (ImGui::Selectable(GetEmissionLabel(i), isSelected)) {
          emissionIdx = i;
          render.ClearImageBuffers();
        }
      }
      ImGui::EndCombo();
    }
  }
  ImGui::End();
}

} // -- anon namespace

extern "C" {

char const * PluginLabel() { return "base UI"; }
mt::PluginType PluginType() { return mt::PluginType::UserInterface; }

void Dispatch(
  mt::core::Scene & scene
, mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  ::UiCameraControls(scene, plugin, render);
  ::UiPluginInfo(scene, render, plugin);
  ::UiImageOutput(scene, render, plugin);
  ::UiEmitters(scene, render, plugin);
  ::UiDispatchers(render, plugin);
  ::UiTextureEditor(scene);
  ::UiKernelDispatchEditor(render, plugin);
}

} // -- extern C
