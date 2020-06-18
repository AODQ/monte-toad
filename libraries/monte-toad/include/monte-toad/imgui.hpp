#pragma once

#include <cstdint>

#include <imgui/imgui.hpp>

namespace ImGui {
  bool InputInt(const char * label, size_t * value, int step = 1);
  bool InputInt(const char * label, uint16_t * value, int step = 1);
  bool InputInt2(const char * label, uint16_t * value, int step = 1);
}
