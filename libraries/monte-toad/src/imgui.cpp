#include <monte-toad/imgui.hpp>

bool ImGui::InputInt(const char * label, size_t * value) {
  int valueAsInt = static_cast<int>(*value);
  bool result = ImGui::InputInt(label, &valueAsInt);
  *value = static_cast<size_t>(valueAsInt);
  return result;
}

bool ImGui::InputInt2(const char * label, uint16_t * value) {
  int values[2] = { static_cast<int>(value[0]), static_cast<int>(value[1]) };
  bool result = ImGui::InputInt2(label, values);
  if (result) {
    value[0] = static_cast<uint16_t>(values[0]);
    value[1] = static_cast<uint16_t>(values[1]);
  }
  return result;
}
