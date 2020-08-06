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
      std::array<char, 8192ul> filename;
      fgets(filename.data(), 8192ul, file);
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

std::vector<std::string> mt::util::FilePickerMultiple(
  std::string const & flags
) {
  std::string const packedFiles =
      mt::util::FilePicker(flags + std::string{" --multiple --separator='|'"})
    + "|"
  ;
  std::vector<std::string> files;

  // avoid empty string files check that a file was chosen
  if (packedFiles == "|") { return {}; }

  // split string using | delimiter
  for (
    size_t start = 0lu, end;
    (end = packedFiles.find_first_of('|', start)) != std::string::npos;
  ) {
    files.emplace_back(packedFiles.substr(start, end - start));
    start = end+1;
  }

  return files;
}
