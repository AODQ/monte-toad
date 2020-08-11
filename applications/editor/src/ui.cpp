#include "ui.hpp"

#include "fileutil.hpp"
#include "graphicscontext.hpp"

#include <monte-toad/core/glutil.hpp>
#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/util/file.hpp>
#include <monte-toad/util/textureloader.hpp>
#include <mt-plugin-host/plugin.hpp>
#include <mt-plugin/plugin.hpp>

#include <glad/glad.hpp>
#include <GLFW/glfw3.h>
#include <imgui/imgui.hpp>
#include <imgui/imgui_impl_glfw.hpp>
#include <imgui/imgui_impl_opengl3.hpp>
#include <omp.h>
#include <spdlog/sinks/base_sink.h>

#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

namespace ImGui {
  bool InputInt(const char * label, size_t * value, int step) {
    int valueAsInt = static_cast<int>(*value);
    bool result = ImGui::InputInt(label, &valueAsInt, step);
    *value = static_cast<size_t>(valueAsInt);
    return result;
  }

  bool InputInt(const char * label, uint16_t * value, int step) {
    int valueAsInt = static_cast<int>(*value);
    bool result = ImGui::InputInt(label, &valueAsInt, step);
    *value = static_cast<uint16_t>(valueAsInt);
    return result;
  }

  bool InputInt2(const char * label, uint16_t * value, int step) {
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
mt::core::Scene scene;

bool reloadPlugin = false;

struct GuiLogMessage {
  GuiLogMessage() = default;
  std::string preLevel, colorLevel, postLevel;
  spdlog::level::level_enum level;
};

//------------------------------------------------------------------------------
class GuiSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  std::deque<GuiLogMessage> logMessages;
  size_t maxMessages = 64;
  bool newMessage = false;
  void sink_it_(spdlog::details::log_msg const & msg) override {
    spdlog::memory_buf_t formatted;
    this->formatter_->format(msg, formatted);

    GuiLogMessage logMsg;
    auto str = fmt::to_string(formatted);
    logMsg.preLevel = str.substr(0, msg.color_range_start);
    logMsg.colorLevel =
      str.substr(
        msg.color_range_start
      , msg.color_range_end - msg.color_range_start
      );
    logMsg.postLevel = str.substr(msg.color_range_end, std::string::npos);
    logMsg.level = msg.level;
    this->logMessages.emplace_back(logMsg);
    newMessage = true;
    flush_();
  }

  void flush_() override {
    // truncate old messages
    while (logMessages.size() > maxMessages) { logMessages.pop_front(); }
  }
};

std::shared_ptr<GuiSink> imGuiSink;

void LoadScene(mt::core::RenderInfo & render, mt::PluginInfo & plugin) {
  if (!mt::Valid(plugin, mt::PluginType::AccelerationStructure)) {
    spdlog::error(
      "Need to have an acceleration structure plugin in order to load the scene"
    );
    return;
  }

  mt::core::Scene::Construct(
    ::scene
  , plugin
  , render.modelFile
  );

  ::scene.emissionSource.environmentMap =
    mt::util::LoadTexture(render.environmentMapFile);

  for (auto & emitter : plugin.emitters)
    { emitter.Precompute(scene, render, plugin); }
}

////////////////////////////////////////////////////////////////////////////////
void AllocateResources(mt::core::RenderInfo & render) {
  for (auto & integrator : render.integratorData)
    { mt::core::AllocateResources(integrator); }
}

////////////////////////////////////////////////////////////////////////////////
[[maybe_unused]]
void UiPluginLoadFile(
  mt::PluginInfo & plugin
, mt::core::RenderInfo & render
, mt::PluginType pluginType
, std::string & /*file*/
) {
  std::string tempFile =
    mt::util::FilePicker(" --file-filter=\"mt-plugin | *.mt-plugin\"");

  // only set to the plugin string if the filesize > 0 & path seems valid
  // also remove newline while we're at it
  if (tempFile.size() == 0 || tempFile[0] != '/') {
    spdlog::info("Did not load any plugin");
    return;
  }

  if (fileutil::LoadPlugin(plugin, render, tempFile, pluginType)) {
    // load plugin w/ scene etc
    switch (pluginType) {
      default: break;
      case mt::PluginType::Integrator:
        mt::core::AllocateResources(render.integratorData.back());
      break;
      case mt::PluginType::Random:
        plugin.random.Initialize();
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void UiPlugin(mt::PluginInfo & plugin) {
  ImGui::Begin("Plugins");
    auto DisplayPluginUi = [&](mt::PluginType pluginType, size_t idx = 0) {
      ImGui::Text("%s (%lu)", ToString(pluginType), idx);
      if (!mt::Valid(plugin, pluginType, idx)) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Not loaded");
      }
      ImGui::Separator();
    };

  for (size_t idx = 0; idx < plugin.integrators.size(); ++ idx)
    { DisplayPluginUi(mt::PluginType::Integrator, idx); }

  for (size_t idx = 0; idx < plugin.bsdfs.size(); ++ idx)
    { DisplayPluginUi(mt::PluginType::Bsdf, idx); }

  DisplayPluginUi(mt::PluginType::AccelerationStructure);
  DisplayPluginUi(mt::PluginType::Camera);
  DisplayPluginUi(mt::PluginType::Dispatcher);
  DisplayPluginUi(mt::PluginType::Emitter);
  DisplayPluginUi(mt::PluginType::Kernel);
  DisplayPluginUi(mt::PluginType::Material);
  DisplayPluginUi(mt::PluginType::Random);
  DisplayPluginUi(mt::PluginType::UserInterface);

  ImGui::End();
}

//------------------------------------------------------------------------------
void UiLog() {
  ImGui::Begin("Console Log");

    // print out every message to terminal; have to determine colors and labels
    // as well for each warning level
    for (auto const & msg : ::imGuiSink->logMessages) {
      ImVec4 col;
      switch (msg.level) {
        default: col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;
        case spdlog::level::level_enum::critical:
          col = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        break;
        case spdlog::level::level_enum::err:
          col = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
        break;
        case spdlog::level::level_enum::debug:
          col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        break;
        case spdlog::level::level_enum::info:
          col = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
        break;
        case spdlog::level::level_enum::warn:
          col = ImVec4(0.8f, 0.8f, 0.3f, 1.0f);
        break;
        case spdlog::level::level_enum::trace:
          col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        break;
      }
      ImGui::Text("%s", msg.preLevel.c_str());
      ImGui::SameLine(0, 0);
      ImGui::TextColored(col, "%s", msg.colorLevel.c_str());
      ImGui::SameLine(0, 0);
      ImGui::Text("%s", msg.postLevel.c_str());
    }

    ImGui::Separator();
    if (ImGui::Button("Clear log")) {
      ::imGuiSink->logMessages.clear();
    }

    if (::imGuiSink->newMessage && !ImGui::IsAnyMouseDown()) {
      ::imGuiSink->newMessage = false;
      ImGui::SetScrollHere(1.0f);
    }

  ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
void UiRenderInfo(
  mt::core::RenderInfo & render
, mt::PluginInfo & plugin
) {
  // -- menubar
  // TODO DTOADQ this causes crash
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Load Scene")) {
      auto tempFilename =
        mt::util::FilePicker(
          " --file-filter=\"3D models |"
          " *.obj *.gltf *.fbx *.stl *.ply *.blend *.dae\""
        );
      if (tempFilename != "") {
        render.modelFile = tempFilename;
        LoadScene(render, plugin);
        // reset camera origin
        render.camera.origin = glm::vec3(0.0f);
        mt::core::UpdateCamera(plugin, render);
      }
    }

    if (ImGui::MenuItem("Load Environment File")) {
      auto tempFilename =
        mt::util::FilePicker(
          " --file-filter=\"image files | "
          " *.jpeg *.jpg *.png *.tga *.bmp *.psd *.gif *.hdr *.pic *.ppm"
          " *.pgm\""
        );
      if (tempFilename != "") {
        render.environmentMapFile = std::filesystem::path{tempFilename};
        LoadScene(render, plugin);
      }
    }

    // TODO
    /* if (ImGui::MenuItem("Save Image")) { */
    /*   auto filenameFlag = */
    /*     render.outputFile == "" ? "" : "--filename " + renderInfo.outputFile; */
    /*   auto tempFilename = */
    /*     mt::util::FilePicker( */
    /*       " --save --file-filter=\"ppm | *.ppm\" " + filenameFlag */
    /*     ); */
    /*   if (tempFilename != "") { */
    /*     render.outputFile = tempFilename; */
    /*     mt::SaveImage( */
    /*       mappedImageTransitionBuffer */
    /*     , render.imageResolution[0], renderInfo.imageResolution[1] */
    /*     , render.outputFile */
    /*     , false */
    /*     ); */
    /*   } */
    /* } */

    ImGui::EndMenu();
  }

  // -- window
  ImGui::Begin("RenderInfo");
    ImGui::Text("'%s'", render.modelFile.c_str());

    if (ImGui::Button("Reload scene")) {
      LoadScene(render, plugin);
    }

    if (ImGui::Button("Reload plugins")) {
     reloadPlugin = true;
    }

    int tempNumThreads = static_cast<int>(render.numThreads);
    if (ImGui::InputInt("# threads", &tempNumThreads)) {
      render.numThreads = static_cast<size_t>(glm::max(1, tempNumThreads));
      omp_set_num_threads(static_cast<int32_t>(render.numThreads));
    }

  ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
void DispatchRender(
  mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  // check if user wants to render anything
  if (!render.globalRendering) { return; }

  // make sure plugin is valid
  if (plugin.integrators.size() == 0) { return; }
  if (plugin.dispatchers.size() == 0) { return; }
  if (!mt::Valid(plugin, mt::PluginType::AccelerationStructure)) { return; }

  plugin
    .dispatchers[render.primaryDispatcher]
    .DispatchRender(render, ::scene, plugin);
}

////////////////////////////////////////////////////////////////////////////////
void UiEntry(
  mt::core::RenderInfo & render
, mt::PluginInfo & plugin
) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(app::DisplayWidth(), app::DisplayHeight()));
  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin(
    "background"
  , nullptr
  ,   ImGuiWindowFlags_NoDecoration
    | ImGuiWindowFlags_NoScrollWithMouse
    | ImGuiWindowFlags_NoBringToFrontOnFocus
    | ImGuiWindowFlags_MenuBar
    | ImGuiWindowFlags_NoCollapse
    | ImGuiWindowFlags_NoResize
    | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoNavFocus
    | ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoDocking
  );

  static auto const dockId = ImGui::GetID("DockBackground");
  ImGui::DockSpace(
    dockId
  , ImVec2(0.0f, 0.0f)
  , ImGuiDockNodeFlags_PassthruCentralNode
  );
  ImGui::PopStyleVar(3);

  ImGui::BeginMenuBar();
  ::UiLog();
  ::UiPlugin(plugin);
  ::UiRenderInfo(render, plugin);

  if (plugin.userInterface.Dispatch)
    { plugin.userInterface.Dispatch(scene, render, plugin); }

  for (size_t idx = 0; idx < plugin.integrators.size(); ++ idx) {
    auto & integrator = plugin.integrators[idx];
    if (integrator.UiUpdate) {
      integrator.UiUpdate(scene, render, plugin, render.integratorData[idx]);
    }
  }

  if (
      plugin.dispatchers.size() > 0
   && plugin.dispatchers[render.primaryDispatcher].UiUpdate
 ) {
    plugin
      .dispatchers[render.primaryDispatcher]
      .UiUpdate(scene, render, plugin);
  }

  if (plugin.material.UiUpdate)
    { plugin.material.UiUpdate(scene, render, plugin); }

  if (plugin.accelerationStructure.UiUpdate)
    { plugin.accelerationStructure.UiUpdate(scene, render, plugin); }

  if (plugin.camera.UiUpdate)
    { plugin.camera.UiUpdate(scene, render, plugin); }

  if (plugin.random.UiUpdate)
    { plugin.random.UiUpdate(scene, render, plugin); }

  for (size_t idx = 0; idx < plugin.emitters.size(); ++ idx) {
    plugin.emitters[idx].UiUpdate(scene, render, plugin);
  }

  ImGui::EndMenuBar();

  ImGui::End();
}

} // -- namespace

void ui::InitializeLogger() {
  ::imGuiSink = std::make_shared<::GuiSink>();
  spdlog::default_logger()->sinks().push_back(::imGuiSink);
}

////////////////////////////////////////////////////////////////////////////////
bool ui::Initialize(
  mt::core::RenderInfo & render
, mt::PluginInfo & plugin
) {
  if (!app::InitializeGraphicsContext()) { return false; }

  { // -- initialize imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();
    io.ConfigFlags =
      0
    /* ImGuiConfigFlags_NavEnableKeyboard */ // conflicts with editor keyboard
    | ImGuiConfigFlags_DockingEnable
    /* | ImGuiConfigFlags_ViewportsEnable */
    ;

    ImGui::StyleColorsDark();

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      ImGui::GetStyle().WindowRounding = 0.0f;
      ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(app::DisplayWindow(), true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
  }

  if (render.modelFile != "")
  { // -- load scene
    LoadScene(render, plugin);
  }

  // load up opengl resources
  ::AllocateResources(render);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
void ui::Run(mt::core::RenderInfo & render, mt::PluginInfo & plugin) {

  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  render.glfwWindow = reinterpret_cast<void*>(app::DisplayWindow());

  while (!glfwWindowShouldClose(app::DisplayWindow())) {
    { // -- event & sleep update
      // check if currently rendering anything
      bool rendering = false;
      for (auto & integrator : render.integratorData) {
        rendering |=
            !integrator.renderingFinished
         && integrator.renderingState != mt::RenderingState::Off
        ;
      }

      // if no rendering is being done right now the thread should just sleep
      rendering = rendering && render.globalRendering;

      // switch between a live event handler or an event-based handler if
      // rendering has occured or not. This saves CPU cycles when monte-toad is
      // just sitting in the background
      if (rendering) {
        glfwPollEvents();
      } else {
        glfwWaitEventsTimeout(1.0);
      }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ::UiEntry(render, plugin);

    ::DispatchRender(render, plugin);

    ImGui::Render();

    // -- validate display size in case of resize
    glfwGetFramebufferSize(
      app::DisplayWindow(), &app::DisplayWidth(), &app::DisplayHeight()
    );
    glViewport(0, 0, app::DisplayWidth(), app::DisplayHeight());

    // clear screen to initiate imgui rendering
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(app::DisplayWindow());
    }

    glfwSwapBuffers(app::DisplayWindow());

    if (reloadPlugin) {
      // update plugins, should be ran every frame with a file checker in the
      // future
      mt::UpdatePlugins(plugin);
      reloadPlugin = false;
    }
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(app::DisplayWindow());
  glfwTerminate();

  mt::FreePlugins();
}
