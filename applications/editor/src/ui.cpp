#include "ui.hpp"

#include "glutil.hpp"

#include <monte-toad/engine.hpp>
#include <monte-toad/imagebuffer.hpp>
#include <monte-toad/log.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/span.hpp>
#include <mt-plugin-host/plugin.hpp>
#include <mt-plugin/plugin.hpp>

#include <glad/glad.hpp>
#include <GLFW/glfw3.h>
#include <imgui/imgui.hpp>
#include <imgui/imgui_impl_glfw.hpp>
#include <imgui/imgui_impl_opengl3.hpp>

#include <filesystem>
#include <string>

#include <cr/cr.h>

namespace {
GLFWwindow* window;
mt::Scene scene;

GlBuffer imageTransitionBuffer;
span<glm::vec4> mappedImageTransitionBuffer;
GlTexture renderedTexture;
GlProgram imageTransitionProgram;

float imguiScale = 1.0f;
int displayWidth, displayHeight;

bool rendering = false;

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
  std::string tempFilename =
    ::FilePicker(" --file-filter=\"mt-plugin | *.mt-plugin\"");

  // only set to the plugin string if the filesize > 0 & path seems valid
  // also remove newline while we're at it
  if (tempFilename.size() != 0 && tempFilename[0] == '/') {
    file = tempFilename;
    switch (mt::LoadPlugin(pluginInfo, pluginType, file)) {
      case CR_NONE:
        spdlog::info("Loaded '{}', no error");
      break;
      case CR_INITIAL_FAILURE:
        spdlog::error("Could not load '{}', initial failure", file);
      break;
      case CR_SEGFAULT:
        spdlog::error("Could not load '{}', Segfault", file);
      break;
      case CR_ILLEGAL:
        spdlog::error("Could not load '{}', Illegal operation", file);
      break;
      case CR_ABORT:
        spdlog::error("Could not load '{}', aborted, SIGBRT", file);
      break;
      case CR_MISALIGN:
        spdlog::error("Could not load '{}', misalignment, SIGBUS", file);
      break;
      case CR_BOUNDS:
        spdlog::error("Could not load '{}', bounds error", file);
      break;
      case CR_STACKOVERFLOW:
        spdlog::error("Could not load '{}', stack overflow", file);
      break;
      case CR_STATE_INVALIDATED:
        spdlog::error("Could not load '{}', static CR_STATE failure", file);
      break;
      case CR_BAD_IMAGE:
        spdlog::error("Could not load '{}', plugin is not valid", file);
      break;
      case CR_OTHER:
        spdlog::error("Could not load '{}', other signal", file);
      break;
      case CR_USER:
        spdlog::error("Could not load '{}', user/mt plugin error", file);
      break;
      default:
        spdlog::error("Could not load '{}', unknown failure", file);
      break;
    }
    if (!mt::Valid(pluginInfo, pluginType))
      { spdlog::error("Failed to load plugin, or plugin is incomplete"); }
  } else {
    spdlog::info("Did not load any plugin");
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
    break;
    case mt::PluginType::Kernel:
    break;
    case mt::PluginType::Material:
    break;
    case mt::PluginType::Camera:
    break;
    case mt::PluginType::Random:
    break;
    case mt::PluginType::UserInterface:
      ImGui::Text(
        "Dispatch %p",
        reinterpret_cast<void*>(info.userInterface.Dispatch)
      );
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

////////////////////////////////////////////////////////////////////////////////
void UiConfig() {
  static bool open = true;
  if (!ImGui::Begin("UiConfig", &open)) { return; }
  ImGui::SetWindowFontScale(::imguiScale);

  static int32_t selectedScale = 1;
  ImGui::RadioButton("Scale 0.5x", &selectedScale, 0);
  ImGui::RadioButton("Scale 1.0x", &selectedScale, 1);
  ImGui::RadioButton("Scale 2.0x", &selectedScale, 2);
  ImGui::RadioButton("Scale 4.0x", &selectedScale, 3);

  switch (selectedScale) {
    case 0: ::imguiScale = 0.5f; break;
    case 1: ::imguiScale = 1.0f; break;
    case 2: ::imguiScale = 2.0f; break;
    case 3: ::imguiScale = 4.0f; break;
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
  ImGui::SetWindowFontScale(::imguiScale);

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

  ImGui::Text("Thread usage %lu", renderInfo.numThreads);

  ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
void UiSceneInfo() {
  static bool open = true;
  if (!ImGui::Begin("SceneInfo", &open)) { return; }
  ImGui::SetWindowFontScale(::imguiScale);

  ImGui::Text(
    "(%f, %f, %f) -> (%f, %f, %f) scene bounds"
  , ::scene.bboxMin.x, ::scene.bboxMin.y, ::scene.bboxMin.z
  , ::scene.bboxMax.x, ::scene.bboxMax.y, ::scene.bboxMax.z
  );
  ImGui::Text("%lu textures", ::scene.textures.size());
  ImGui::Text("%lu meshes", ::scene.meshes.size());

  ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////
void UiImageOutput(mt::RenderInfo & renderInfo) {
  static bool open = true;
  if (!ImGui::Begin("ImageOutput", &open)) { return; }

  ImGui::Image(
    reinterpret_cast<ImTextureID>(::renderedTexture.handle)
  , ImVec2(renderInfo.imageResolution[0], renderInfo.imageResolution[1])
  );

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
  ::UiConfig();
  ::UiPlugin(pluginInfo);
  ::UiRenderInfo(renderInfo, pluginInfo);
  ::UiSceneInfo();
  ::UiImageOutput(renderInfo);

  if (pluginInfo.userInterface.Dispatch) {
    pluginInfo
      .userInterface
      .Dispatch(scene, renderInfo, pluginInfo);
  }

  ImGui::EndMenuBar();

  ImGui::End();
}

} // -- end anon namespace

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
