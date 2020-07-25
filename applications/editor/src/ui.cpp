#include "ui.hpp"

#include "fileutil.hpp"
#include "graphicscontext.hpp"

#include <monte-toad/core/integratordata.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/glutil.hpp>
#include <mt-plugin-host/plugin.hpp>
#include <mt-plugin/plugin.hpp>

#include <glad/glad.hpp>
#include <GLFW/glfw3.h>
#include <imgui/imgui.hpp>
#include <imgui/imgui_impl_glfw.hpp>
#include <imgui/imgui_impl_opengl3.hpp>
#include <spdlog/sinks/base_sink.h>

#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

#include <omp.h>

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

mt::GlProgram imageTransitionProgram;

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
  mt::core::Scene::Construct(
    ::scene
  , render.modelFile
  , render.environmentMapFile
  );

  for (auto & emitter : plugin.emitters)
    { emitter.Precompute(scene, render, plugin); }

  if (mt::Valid(plugin, mt::PluginType::Material)) {
    plugin.material.Load(plugin.material, ::scene);
  }
}

////////////////////////////////////////////////////////////////////////////////
void AllocateGlResources(mt::core::RenderInfo & renderInfo) {
  for (auto & integrator : renderInfo.integratorData)
    { mt::core::AllocateGlResources(integrator, renderInfo); }
}

////////////////////////////////////////////////////////////////////////////////
[[maybe_unused]]
void UiPluginLoadFile(
  mt::PluginInfo & pluginInfo
, mt::core::RenderInfo & render
, mt::PluginType pluginType
, std::string & /*file*/
) {
  std::string tempFile =
    fileutil::FilePicker(" --file-filter=\"mt-plugin | *.mt-plugin\"");

  // only set to the plugin string if the filesize > 0 & path seems valid
  // also remove newline while we're at it
  if (tempFile.size() == 0 || tempFile[0] != '/') {
    spdlog::info("Did not load any plugin");
    return;
  }

  if (fileutil::LoadPlugin(pluginInfo, render, tempFile, pluginType)) {
    // load plugin w/ scene etc
    switch (pluginType) {
      default: break;
      case mt::PluginType::Integrator:
        mt::core::AllocateGlResources(render.integratorData.back(), render);
      break;
      case mt::PluginType::Material:
        pluginInfo.material.Load(pluginInfo.material, scene);
      break;
      case mt::PluginType::Random:
        pluginInfo.random.Initialize();
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
[[maybe_unused]]
void UiPluginDisplayInfo(
  mt::PluginInfo const & plugin
, mt::PluginType pluginType
, size_t idx = 0
) {
  switch (pluginType) {
    case mt::PluginType::Integrator: {
      auto & integrator = plugin.integrators[idx];
      ImGui::Text("Realtime: %s", integrator.RealTime() ? "Yes" : "No");
    } break;
    case mt::PluginType::Kernel: break;
    case mt::PluginType::Material: break;
    case mt::PluginType::Camera: break;
    case mt::PluginType::Random: break;
    case mt::PluginType::UserInterface: break;
    case mt::PluginType::Emitter: break;
    case mt::PluginType::Dispatcher: break;
    default: break;
  }
}

////////////////////////////////////////////////////////////////////////////////
void UiPlugin(mt::PluginInfo & pluginInfo) {
  ImGui::Begin("Plugins");
    auto DisplayPluginUi = [&](mt::PluginType pluginType, size_t idx = 0) {
      ImGui::Text("%s (%lu)", ToString(pluginType), idx);
      if (!mt::Valid(pluginInfo, pluginType, idx)) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Not loaded");
      }
      ImGui::Separator();
    };

    for (size_t idx = 0; idx < pluginInfo.integrators.size(); ++ idx)
      { DisplayPluginUi(mt::PluginType::Integrator, idx); }

    DisplayPluginUi(mt::PluginType::Kernel);
    DisplayPluginUi(mt::PluginType::Material);
    DisplayPluginUi(mt::PluginType::Camera);
    DisplayPluginUi(mt::PluginType::Random);
    DisplayPluginUi(mt::PluginType::UserInterface);
    DisplayPluginUi(mt::PluginType::Emitter);
    DisplayPluginUi(mt::PluginType::Dispatcher);

  ImGui::End();

  /* // try to iterate plugin name information in a way that it doesn't have to be */
  /* // manually maintained */
  /* for (size_t i = 0; i < static_cast<size_t>(mt::PluginType::Size); ++ i) { */
  /*   auto pluginType = static_cast<mt::PluginType>(i); */

  /*   std::string const infoStr = */
  /*     std::string{ToString(pluginType)} */
  /*   + "(" + pluginPath.filename().string() + ")"; */

  /*   std::string const buttonStr = "Load##" + std::string{ToString(pluginType)}; */

  /*   auto treeNodeResult = ImGui::TreeNode(infoStr.c_str()); */

  /*   if (!mt::Valid(pluginInfo, pluginType)) { */
  /*     ImGui::SameLine(); */
  /*     ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Not loaded"); */
  /*   } */

  /*   if (treeNodeResult) { */
  /*     UiPluginDisplayInfo(pluginInfo, pluginType); */
  /*     ImGui::TreePop(); */
  /*   } */

  /*   if (ImGui::Button(buttonStr.c_str())) */
  /*     { UiPluginLoadFile(pluginInfo, pluginType, pluginName); } */

  /*   ImGui::Separator(); */
  /* } */
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
  mt::core::RenderInfo & renderInfo
, mt::PluginInfo & pluginInfo
) {
  // -- menubar
  // TODO DTOADQ this causes crash
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Load Scene")) {
      auto tempFilename =
        fileutil::FilePicker(
          " --file-filter=\"3D models |"
          " *.obj *.gltf *.fbx *.stl *.ply *.blend *.dae\""
        );
      if (tempFilename != "") {
        renderInfo.modelFile = tempFilename;
        LoadScene(renderInfo, pluginInfo);
        // reset camera origin
        renderInfo.camera.origin = glm::vec3(0.0f);
        mt::core::UpdateCamera(pluginInfo, renderInfo);
      }
    }

    if (ImGui::MenuItem("Load Environment File")) {
      auto tempFilename =
        fileutil::FilePicker(
          " --file-filter=\"image files | "
          " *.jpeg *.jpg *.png *.tga *.bmp *.psd *.gif *.hdr *.pic *.ppm"
          " *.pgm\""
        );
      if (tempFilename != "") {
        renderInfo.environmentMapFile = std::filesystem::path{tempFilename};
        LoadScene(renderInfo, pluginInfo);
      }
    }

    // TODO
    /* if (ImGui::MenuItem("Save Image")) { */
    /*   auto filenameFlag = */
    /*     renderInfo.outputFile == "" ? "" : "--filename " + renderInfo.outputFile; */
    /*   auto tempFilename = */
    /*     fileutil::FilePicker( */
    /*       " --save --file-filter=\"ppm | *.ppm\" " + filenameFlag */
    /*     ); */
    /*   if (tempFilename != "") { */
    /*     renderInfo.outputFile = tempFilename; */
    /*     mt::SaveImage( */
    /*       mappedImageTransitionBuffer */
    /*     , renderInfo.imageResolution[0], renderInfo.imageResolution[1] */
    /*     , renderInfo.outputFile */
    /*     , false */
    /*     ); */
    /*   } */
    /* } */

    ImGui::EndMenu();
  }

  // -- window
  ImGui::Begin("RenderInfo");
    ImGui::Text("'%s'", renderInfo.modelFile.c_str());

    if (ImGui::Button("Reload scene")) {
      LoadScene(renderInfo, pluginInfo);
    }

    if (ImGui::Button("Reload plugins")) {
     reloadPlugin = true;
    }

    int tempNumThreads = static_cast<int>(renderInfo.numThreads);
    if (ImGui::InputInt("# threads", &tempNumThreads)) {
      renderInfo.numThreads = static_cast<size_t>(glm::max(1, tempNumThreads));
      omp_set_num_threads(static_cast<int32_t>(renderInfo.numThreads));
    }

  ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
void DispatchRender(
  mt::core::RenderInfo & render
, mt::PluginInfo const & plugin
) {
  // make sure plugin is valid
  if (plugin.integrators.size() == 0) { return; }

  glUseProgram(::imageTransitionProgram.handle);

  // iterate through all integrators and run either their low or high quality
  // dispatches
  for (size_t idx = 0; idx < plugin.integrators.size(); ++ idx) {
    auto & integratorData = render.integratorData[idx];

    if (mt::core::DispatchRender(integratorData, ::scene, render, plugin, idx))
    {
      FlushTransitionBuffer(integratorData);
      mt::core::DispatchImageCopy(integratorData);
    }
  }
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

  for (size_t idx = 0; idx < plugin.integrators.size(); ++ idx) {
    auto & integrator = plugin.integrators[idx];
    if (integrator.UiUpdate) {
      integrator.UiUpdate(scene, render, plugin, render.integratorData[idx]);
    }
  }

  if (plugin.kernel.UiUpdate)
    { plugin.kernel.UiUpdate(scene, render, plugin); }

  if (plugin.material.UiUpdate)
    { plugin.material.UiUpdate(scene, render, plugin); }

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
  ::AllocateGlResources(render);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
void ui::Run(mt::core::RenderInfo & renderInfo, mt::PluginInfo & pluginInfo) {

  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  renderInfo.glfwWindow = reinterpret_cast<void*>(app::DisplayWindow());

  while (!glfwWindowShouldClose(app::DisplayWindow())) {
    { // -- event & sleep update
      // check if currently rendering anything
      bool rendering = false; // TODO TOAD set to rendering
      for (auto & integrator : renderInfo.integratorData) {
        rendering = rendering | !integrator.renderingFinished;
      }

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

    ::UiEntry(renderInfo, pluginInfo);

    ::DispatchRender(renderInfo, pluginInfo);

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
      mt::UpdatePlugins(pluginInfo);
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
