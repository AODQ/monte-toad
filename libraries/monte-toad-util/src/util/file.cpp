#include <monte-toad/util/file.hpp>

#include <array>

std::string mt::util::FilePicker(std::string const & flags) {
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

