#include "ui.hpp"

#include "glutil.hpp"

#include <monte-toad/engine.hpp>
#include <monte-toad/imagebuffer.hpp>
#include <monte-toad/enum.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/span.hpp>
#include <mt-plugin-host/plugin.hpp>
#include <mt-plugin/plugin.hpp>

#include <cr/cr.h>
#include <glad/glad.hpp>
#include <GLFW/glfw3.h>
#include <imgui/imgui.hpp>
#include <imgui/imgui_impl_glfw.hpp>
#include <imgui/imgui_impl_opengl3.hpp>
#include <spdlog/sinks/base_sink.h>

#include <filesystem>
#include <mutex>
#include <string>

namespace {
GLFWwindow* window;
mt::Scene scene;

GlBuffer imageTransitionBuffer;
span<glm::vec4> mappedImageTransitionBuffer;
GlTexture renderedTexture;
GlProgram imageTransitionProgram;

mt::DiagnosticInfo diagnosticInfo;

int displayWidth, displayHeight;

bool rendering = false;

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
  }

  void flush_() override {
    // truncate old messages
    while (logMessages.size() > maxMessages) { logMessages.pop_front(); }
  }
};

std::shared_ptr<GuiSink> imGuiSink;

////////////////////////////////////////////////////////////////////////////////
void AllocateGlResources(mt::RenderInfo const & renderInfo) {
  // free resources beforehand in case buffers are being reallocated, ei for
  // texture resize
  ::imageTransitionBuffer.Free();
  ::renderedTexture.Free();
  ::imageTransitionProgram.Free();

  size_t const
    imagePixelLength =
      renderInfo.imageResolution[0] * renderInfo.imageResolution[1]
  , imageByteLength =
      imagePixelLength * sizeof(glm::vec4);

  // -- construct transition buffer
  ::imageTransitionBuffer.Construct();
  glNamedBufferStorage(
    ::imageTransitionBuffer.handle
  , imageByteLength
  , nullptr
  , GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT
  );

  ::mappedImageTransitionBuffer =
    span<glm::vec4>(
      reinterpret_cast<glm::vec4*>(
        glMapNamedBufferRange(
          ::imageTransitionBuffer.handle
        , 0, imageByteLength
        ,   GL_MAP_WRITE_BIT
          | GL_MAP_PERSISTENT_BIT
          | GL_MAP_INVALIDATE_RANGE_BIT
          | GL_MAP_INVALIDATE_BUFFER_BIT
          | GL_MAP_FLUSH_EXPLICIT_BIT
        )
      ),
      imagePixelLength
    );
  ::diagnosticInfo.currentFragmentBuffer = ::mappedImageTransitionBuffer.data();

  // -- construct texture
  ::renderedTexture.Construct(GL_TEXTURE_2D);
  glTextureStorage2D(
    ::renderedTexture.handle
  , 1, GL_RGBA8
  , renderInfo.imageResolution[0], renderInfo.imageResolution[1]
  );
  { // -- set parameters
    auto const & handle = ::renderedTexture.handle;
    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  }

  glBindTextureUnit(0, ::renderedTexture.handle);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ::imageTransitionBuffer.handle);
  glBindImageTexture(
    0, ::renderedTexture.handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8
  );

  ::diagnosticInfo.textureHandle =
    reinterpret_cast<void*>(::renderedTexture.handle);

  // -- construct program
  std::string const imageTransitionSource =
    "#version 460 core\n"
    "layout(local_size_x = 8, local_size_y = 8, local_size_x = 1) in;\n"
    "\n"
    "uniform layout(rgba8, binding = 0) writeonly image2D outputImg;\n"
    "layout(std430, binding = 0) buffer TransitionBuffer {\n"
    "  vec4 color[];\n"
    "} transition;\n"
    "\n"
    "void main(void) {\n"
    "  ivec2 compCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "  vec2 uvCoord = compCoord / vec2(gl_NumWorkGroups*gl_WorkGroupSize).xy;\n"
    "  vec4 color = \n"
    "    transition.color[\n"
    "      compCoord.y*(gl_NumWorkGroups*gl_WorkGroupSize).x + compCoord.x\n"
    "  ];\n"
    "  imageStore(outputImg, compCoord, vec4(color));\n"
    "}\n"
  ;

  ::imageTransitionProgram
    .Construct({{imageTransitionSource, GL_COMPUTE_SHADER}});
}

////////////////////////////////////////////////////////////////////////////////
std::string FilePicker(std::string const & flags) {
  std::string tempFilename = "";
  #ifdef __unix__
    { // use zenity in UNIX for now
      FILE * file =
        popen(
          ( std::string{"zenity --title \"plugin\" --file-selection"}
          + std::string{" "} + flags
          ).c_str()
        , "r"
        );
      std::array<char, 2048> filename;
      fgets(filename.data(), 2048, file);
      tempFilename = std::string{filename.data()};
      pclose(file);
    }
    if (tempFilename[0] != '/') { return ""; }
    // remove newline
    if (tempFilename.size() > 0 && tempFilename.back() == '\n')
      { tempFilename.pop_back(); }
  #endif
  return tempFilename;
}

////////////////////////////////////////////////////////////////////////////////
void UiPluginLoadFile(
  mt::PluginInfo & pluginInfo
, mt::PluginType pluginType
, std::string & file
) {
  std::string tempFile =
    ::FilePicker(" --file-filter=\"mt-plugin | *.mt-plugin\"");

  // only set to the plugin string if the filesize > 0 & path seems valid
  // also remove newline while we're at it
  if (tempFile.size() == 0 || tempFile[0] != '/') {
    spdlog::info("Did not load any plugin");
    return;
  }

  // clean previous data, and also invalidate it in case the next plugin is
  // invalid but does not write out invalid data
  mt::Clean(pluginInfo, pluginType);

  // load plugin & check for plugin-loading errors
  switch (mt::LoadPlugin(pluginInfo, pluginType, tempFile)) {
    case CR_NONE:
      // only save file if loading was successful
      file = tempFile;
      spdlog::info("Loaded '{}'", tempFile);
    break;
    case CR_INITIAL_FAILURE:
      spdlog::error("Could not load '{}', initial failure", tempFile);
    break;
    case CR_SEGFAULT:
      spdlog::error("Could not load '{}', Segfault", tempFile);
    break;
    case CR_ILLEGAL:
      spdlog::error("Could not load '{}', Illegal operation", tempFile);
    break;
    case CR_ABORT:
      spdlog::error("Could not load '{}', aborted, SIGBRT", tempFile);
    break;
    case CR_MISALIGN:
      spdlog::error("Could not load '{}', misalignment, SIGBUS", tempFile);
    break;
    case CR_BOUNDS:
      spdlog::error("Could not load '{}', bounds error", tempFile);
    break;
    case CR_STACKOVERFLOW:
      spdlog::error("Could not load '{}', stack overflow", tempFile);
    break;
    case CR_STATE_INVALIDATED:
      spdlog::error("Could not load '{}', static CR_STATE failure", tempFile);
    break;
    case CR_BAD_IMAGE:
      spdlog::error("Could not load '{}', plugin is not valid", tempFile);
    break;
    case CR_OTHER:
      spdlog::error("Could not load '{}', other signal", tempFile);
    break;
    case CR_USER:
      spdlog::error("Could not load '{}', user/mt plugin error", tempFile);
    break;
    default:
      spdlog::error("Could not load '{}', unknown failure", tempFile);
    break;
  }

  // check that the plugin loaded itself properly (ei members set properly)
  if (!mt::Valid(pluginInfo, pluginType)) {
    spdlog::error("Failed to load plugin, or plugin is incomplete");
    mt::Clean(pluginInfo, pluginType);
    file = "";
    rendering = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
void UiPluginDisplayInfo(
  mt::PluginInfo & info
, mt::PluginType pluginType
) {
  switch (pluginType) {
    case mt::PluginType::Integrator:
      ImGui::Text("Realtime: %s", info.integrator.realtime ? "Yes" : "No");
      ImGui::Text("UseGpu: %s", info.integrator.useGpu ? "Yes" : "No");
      ImGui::Text(
        "Dispatch %p",
        reinterpret_cast<void*>(info.integrator.Dispatch)
      );
      ImGui::Text("PluginType: %s", ToString(info.integrator.pluginType));
    break;
    case mt::PluginType::Kernel:
    break;
    case mt::PluginType::Material:
    break;
    case mt::PluginType::Camera:
      ImGui::Text(
        "Dispatch %p",
        reinterpret_cast<void*>(info.camera.Dispatch)
      );
      ImGui::Text("PluginType: %s", ToString(info.camera.pluginType));
    break;
    case mt::PluginType::Random:
    break;
    case mt::PluginType::UserInterface:
      ImGui::Text(
        "Dispatch %p",
        reinterpret_cast<void*>(info.userInterface.Dispatch)
      );
      ImGui::Text("PluginType: %s", ToString(info.userInterface.pluginType));
    break;
    default: break;
  }
}

////////////////////////////////////////////////////////////////////////////////
void UiPlugin(mt::PluginInfo & pluginInfo) {
  static bool open = true;
  if (!ImGui::Begin("Plugins", &open)) { return; }

  static std::array<std::string, static_cast<size_t>(mt::PluginType::Size)>
    pluginNames;

  // try to iterate plugin name information in a way that it doesn't have to be
  // manually maintained
  for (size_t i = 0; i < static_cast<size_t>(mt::PluginType::Size); ++ i) {
    auto pluginType = static_cast<mt::PluginType>(i);
    auto & pluginName = pluginNames[i];
    auto pluginPath = std::filesystem::path(pluginName);

    std::string const infoStr =
      std::string{ToString(pluginType)}
    + "(" + pluginPath.filename().string() + ")";

    std::string const buttonStr = "Load##" + std::string{ToString(pluginType)};

    auto treeNodeResult = ImGui::TreeNode(infoStr.c_str());

    if (!mt::Valid(pluginInfo, pluginType)) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Not loaded");
    }

    if (treeNodeResult) {
      UiPluginDisplayInfo(pluginInfo, pluginType);
      ImGui::TreePop();
    }

    if (ImGui::Button(buttonStr.c_str()))
      { UiPluginLoadFile(pluginInfo, pluginType, pluginName); }

    ImGui::Separator();
  }

  ImGui::End();
}

//------------------------------------------------------------------------------
void UiLog() {
  static bool open = true;
  if (!ImGui::Begin("Console Log", &open)) { return; }

  // print out every message to terminal; have to determine colors and labels as
  // well for each warning level
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
    ImGui::Text(msg.preLevel.c_str());
    ImGui::SameLine(0, 0);
    ImGui::TextColored(col, msg.colorLevel.c_str());
    ImGui::SameLine(0, 0);
    ImGui::Text(msg.postLevel.c_str());
  }

  if (::imGuiSink->newMessage && !ImGui::IsAnyMouseDown()) {
    ::imGuiSink->newMessage = false;
    ImGui::SetScrollHere(1.0f);
  }

  ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
void UiRenderInfo(mt::RenderInfo & renderInfo, mt::PluginInfo & pluginInfo) {
  // -- menubar
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Load Scene")) {
      auto tempFilename =
        ::FilePicker(
          " --file-filter=\"3D models |"
          " *.obj *.gltf *.fbx *.stl *.ply *.blend *.dae\""
        );
      if (tempFilename != "") {
        renderInfo.modelFile = tempFilename;
        scene =
          mt::Scene::Construct(
            renderInfo.modelFile
          , renderInfo.environmentMapFile
          , renderInfo.bvhOptimize
          );
      }
    }

    if (ImGui::MenuItem("Load Environment File")) {
      auto tempFilename =
        ::FilePicker(
          " --file-filter=\"image files | "
          " .jpeg .jpg .png .tga .bmp .psd .gif .hdr .pic .ppm .pgm\""
        );
      if (tempFilename != "") {
        renderInfo.environmentMapFile = tempFilename;
        scene =
          mt::Scene::Construct(
            renderInfo.modelFile
          , renderInfo.environmentMapFile
          , renderInfo.bvhOptimize
          );
      }
    }

    if (ImGui::MenuItem("Save Image")) {
      auto filenameFlag =
        renderInfo.outputFile == "" ? "" : "--filename " + renderInfo.outputFile;
      auto tempFilename =
        ::FilePicker(
          " --save --file-filter=\"ppm | *.ppm\" " + filenameFlag
        );
      if (tempFilename != "") {
        renderInfo.outputFile = tempFilename;
        mt::SaveImage(
          mappedImageTransitionBuffer
        , renderInfo.imageResolution[0], renderInfo.imageResolution[1]
        , renderInfo.outputFile
        , false
        );
      }
    }

    ImGui::EndMenu();
  }

  // -- window
  static bool open = true;
  if (!ImGui::Begin("RenderInfo", &open)) { return; }

  ImGui::Text("'%s'", renderInfo.modelFile.c_str());

  if (ImGui::Checkbox("Rendering", &::rendering) && rendering) {
    // check that it's possible to render
    if (!mt::Valid(pluginInfo, mt::PluginType::Camera)) {
      spdlog::error("Need camera plugin in order to render");
      rendering = false;
    }
    if (!mt::Valid(pluginInfo, mt::PluginType::Integrator)) {
      spdlog::error("Need integrator plugin in order to render");
      rendering = false;
    }
    if (
        scene.meshes.size() == 0
     || scene.accelStructure->triangles.size() == 0
    ) {
      spdlog::error("Can't render without loading a scene");
      rendering = false;
    }
  }

  enum struct AspectRatio : int {
    e1_1, e3_2, e4_3, e5_4, e16_9, e16_10, e21_9, eNone, size
  };

  std::array<float, Idx(AspectRatio::size)> constexpr ratioConversion = {{
    1.0f, 3.0f/2.0f, 4.0f/3.0f, 5.0f/4.0f, 16.0f/9.0f, 16.0f/10.0f, 21.0f/9.0f
  , 1.0f
  }};

  std::array<char const *, Idx(AspectRatio::size)> constexpr ratioLabels = {{
    "1x1", "3x2", "4x3", "5x4", "16x9", "16x10", "21x9", "None"
  }};

  static AspectRatio aspectRatio = AspectRatio::e4_3;
  static char const * aspectRatioLabel = ratioLabels[Idx(aspectRatio)];
  static std::array<int, 2> imageResolution = {{
    static_cast<int>(renderInfo.imageResolution[0])
  , static_cast<int>(renderInfo.imageResolution[1])
  }};
  static std::array<int, 2> previousImageResolution = {{
    imageResolution[0], imageResolution[1]
  }};

  if (ImGui::BeginCombo("Aspect Ratio", aspectRatioLabel)) {
    for (size_t i = 0; i < ratioLabels.size(); ++ i) {
      bool isSelected = Idx(aspectRatio) == static_cast<int>(i);
      if (ImGui::Selectable(ratioLabels[i], isSelected)) {
        aspectRatio = static_cast<AspectRatio>(i);
        aspectRatioLabel = ratioLabels[i];

        // update image resolution
        imageResolution[1] = imageResolution[0] / ratioConversion[i];
        previousImageResolution[1] = imageResolution[1];
      }

      if (isSelected) { ImGui::SetItemDefaultFocus(); }
    }
    ImGui::EndCombo();
  }

  if (ImGui::InputInt2("Image resolution", imageResolution.data())) {
    // Only 4K support right now (accidentally typing large number sucks)
    if (imageResolution[0] >= 4096) { imageResolution[0] = 4096; }
    if (imageResolution[1] >= 4096) { imageResolution[1] = 4096; }

    // apply aspect ratio if requested
    if (aspectRatio != AspectRatio::eNone) {
      if (previousImageResolution[0] != imageResolution[0]) {
        imageResolution[1] =
          imageResolution[0] / ratioConversion[Idx(aspectRatio)];
      } else {
        imageResolution[0] =
          imageResolution[1] * ratioConversion[Idx(aspectRatio)];
      }
    }

    renderInfo.imageResolution[0] = static_cast<size_t>(imageResolution[0]);
    renderInfo.imageResolution[1] = static_cast<size_t>(imageResolution[1]);

    previousImageResolution[0] = imageResolution[0];
    previousImageResolution[1] = imageResolution[1];

    // have to reallocate everything now
    AllocateGlResources(renderInfo);
  }

  int tempNumThreads = static_cast<int>(renderInfo.numThreads);
  if (ImGui::InputInt("# threads", &tempNumThreads)) {
    renderInfo.numThreads = static_cast<size_t>(glm::max(1, tempNumThreads));
    omp_set_num_threads(renderInfo.numThreads);
  }

  ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
void DispatchRender(
  mt::RenderInfo & renderInfo
, mt::PluginInfo & pluginInfo
) {
  if (!rendering) { return; }

  // make sure plugin info is valid
  if (pluginInfo.integrator.Dispatch == nullptr) { return; }

  mt::DispatchEngineBlockRegion(
    ::scene
  , ::mappedImageTransitionBuffer
  , renderInfo
  , pluginInfo
  , 0, ::mappedImageTransitionBuffer.size()
  , false
  );

  glFlushMappedNamedBufferRange(
    ::imageTransitionBuffer.handle
  , 0, sizeof(glm::vec4) * ::mappedImageTransitionBuffer.size()
  );

  // TODO perform compute shader
  glBindTextureUnit(0, ::renderedTexture.handle);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ::imageTransitionBuffer.handle);
  glBindImageTexture(
    0, ::renderedTexture.handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8
  );
  glUseProgram(::imageTransitionProgram.handle);

  glDispatchCompute(
    renderInfo.imageResolution[0] / 8,
    renderInfo.imageResolution[1] / 8,
    1
  );
}

////////////////////////////////////////////////////////////////////////////////
void UiEntry(
  mt::Scene & scene
, mt::RenderInfo & renderInfo
, mt::PluginInfo & pluginInfo
) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(::displayWidth, ::displayHeight));
  ImGui::SetNextWindowSize(ImVec2(::displayWidth, ::displayHeight));
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
  ::UiPlugin(pluginInfo);
  ::UiRenderInfo(renderInfo, pluginInfo);

  if (pluginInfo.userInterface.Dispatch) {
    pluginInfo
      .userInterface
      .Dispatch(scene, renderInfo, pluginInfo, ::diagnosticInfo);
  }

  ImGui::EndMenuBar();

  ImGui::End();
}

} // -- end anon namespace

void ui::InitializeLogger() {
  ::imGuiSink = std::make_shared<::GuiSink>();
  spdlog::default_logger()->sinks().push_back(::imGuiSink);
}

////////////////////////////////////////////////////////////////////////////////
bool ui::Initialize(mt::RenderInfo const & renderInfo) {
  if (!glfwInit()) { return false; }

  // opengl 4.6
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  // set up new window to be as non-intrusive as possible. No maximizing,
  // floating, no moving cursor, no auto-focus, etc.
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
  glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
  glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
  glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);
  glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 0);

  glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE);

  { // -- get render resolution TODO allow in settings or something
    int xpos, ypos, width, height;
    glfwGetMonitorWorkarea(
      glfwGetPrimaryMonitor(), &xpos, &ypos, &width, &height
    );

    ::displayWidth = width;
    ::displayHeight = height;
  }

  ::window =
    glfwCreateWindow(
      ::displayWidth, ::displayHeight
    , "monte-toad", nullptr, nullptr
    );

  if (!::window) { glfwTerminate(); return false; }
  glfwMakeContextCurrent(::window);

  // initialize glad
  if (!gladLoadGL()) { return false; }

  glfwSwapInterval(0);

  { // -- initialize imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();
    io.ConfigFlags =
      ImGuiConfigFlags_NavEnableKeyboard
    | ImGuiConfigFlags_DockingEnable
    /* | ImGuiConfigFlags_ViewportsEnable */
    ;

    ImGui::StyleColorsDark();

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      ImGui::GetStyle().WindowRounding = 0.0f;
      ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(::window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");
  }

  if (renderInfo.modelFile != "")
  { // -- load scene
    scene =
      mt::Scene::Construct(
        renderInfo.modelFile
      , renderInfo.environmentMapFile
      , renderInfo.bvhOptimize
      );
  }

  // load up opengl resources
  ::AllocateGlResources(renderInfo);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
void ui::Run(mt::RenderInfo & renderInfo , mt::PluginInfo & pluginInfo) {
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  while (!glfwWindowShouldClose(::window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ::UiEntry(::scene, renderInfo, pluginInfo);

    ::DispatchRender(renderInfo, pluginInfo);

    ImGui::Render();

    // -- validate display size in case of resize
    glfwGetFramebufferSize(::window, &::displayWidth, &::displayHeight);
    glViewport(0, 0, ::displayWidth, ::displayHeight);

    // clear screen to initiate imgui rendering
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(::window);
    }

    glfwSwapBuffers(::window);

    // update plugins too
    mt::UpdatePlugins();
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(::window);
  glfwTerminate();
}
