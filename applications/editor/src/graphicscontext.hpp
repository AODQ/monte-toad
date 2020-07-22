#pragma once

// -- fwd decl
struct GLFWwindow;

namespace app {
  bool InitializeGraphicsContext();
  int & DisplayWidth();
  int & DisplayHeight();
  GLFWwindow * DisplayWindow();
}
