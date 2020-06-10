#include <monte-toad/imgui.hpp>

bool ImGui::InputInt(const char * label, size_t * value) {
  int valueAsInt = static_cast<int>(*value);
  bool result = ImGui::InputInt(label, &valueAsInt);
  *value = static_cast<size_t>(valueAsInt);
  return result;
}
