#pragma once

#include <string>
#include <vector>

namespace mt::util {
  std::string FilePicker(std::string const & flags);
  std::vector<std::string> FilePickerMultiple(std::string const & flags);
}
