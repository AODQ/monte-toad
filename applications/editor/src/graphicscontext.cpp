#include "graphicscontext.hpp"

#include <glad/glad.hpp>
#include <monte-toad/core/log.hpp>

#include <GLFW/glfw3.h>

namespace {
GLFWwindow * displayWindow;

int displayWidth, displayHeight;
} // -- namespace

bool app::InitializeGraphicsContext() {
  spdlog::info("initializing graphics context");
  if (!glfwInit()) {
    spdlog::error("failed to initialize GLFW");
    return false;
  }

  glfwSetErrorCallback([](int, char const * errorStr) {
    spdlog::error("glfw error; '{}'", errorStr);
  });

  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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

  ::displayWindow =
    glfwCreateWindow(
      ::displayWidth, ::displayHeight
    , "monte-toad", nullptr, nullptr
    );

  if (!::displayWindow) {
    spdlog::error("failed to initialize GLFW window");
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(::displayWindow);

  // initialize glad
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    spdlog::error("failed to initialize GLAD");
    return false;
  }

  glfwSwapInterval(0);

  return true;
}

int & app::DisplayWidth() { return ::displayWidth; }
int & app::DisplayHeight() { return ::displayHeight; }
GLFWwindow * app::DisplayWindow() { return ::displayWindow; }
