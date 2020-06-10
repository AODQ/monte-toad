#include "fileutil.hpp"

#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <mt-plugin-host/plugin.hpp>
#include <mt-plugin/plugin.hpp>

#include <cr/cr.h>
#include <nlohmann/json.hpp>

#include <fstream>

//------------------------------------------------------------------------------
void fileutil::LoadEditorConfig(
  mt::RenderInfo & render
, mt::PluginInfo & plugin
) {
  nlohmann::json json;
  { // -- load file
    std::ifstream file("config.json");
    if (file.eof() || !file.good()) { return; }
    file >> json;
  }

  for (auto const & pluginInfo : json["plugins"]) {

    auto info = pluginInfo.get<std::vector<std::string>>();
    mt::PluginType type;

    if (info[0] == "integrator") {
      type = mt::PluginType::Integrator;
    } else if (info[0] == "kernel") {
      type = mt::PluginType::Kernel;
    } else if (info[0] == "material") {
      type = mt::PluginType::Material;
    } else if (info[0] == "camera") {
      type = mt::PluginType::Camera;
    } else if (info[0] == "random") {
      type = mt::PluginType::Random;
    } else if (info[0] == "userinterface") {
      type = mt::PluginType::UserInterface;
    } else {
      spdlog::error("Unknown plugin type '{}' when loading config", info[0]);
      return;
    }

    /* mt::FreePlugins(); */
    fileutil::LoadPlugin(plugin, render, info[1], type);

    // TODO set their configs
  }

  for (auto const & sceneInfo : json["scene"]) {
    render.modelFile = sceneInfo.get<std::string>();
  }
}

//------------------------------------------------------------------------------
void fileutil::SaveEditorConfig(
  mt::RenderInfo const & render
, mt::PluginInfo const & plugin
) {
}

//------------------------------------------------------------------------------
bool fileutil::LoadPlugin(
  mt::PluginInfo & plugin
, mt::RenderInfo & render
, std::string const & file
, mt::PluginType type
) {
  spdlog::info("Loading {} of type {}", file, ToString(type));

  // allocate memory for plugin if it's a vector
  size_t idx = 0;
  if (type == mt::PluginType::Integrator) {
    plugin.integrators.emplace_back();
    render.integratorData.emplace_back();
    idx = plugin.integrators.size()-1;
  }

  mt::Clean(plugin, type, idx);

  switch (mt::LoadPlugin(plugin, type, file, idx)) {
    case CR_NONE:
      spdlog::info("Loaded '{}'", file);
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

  // check that the plugin loaded itself properly (ei members set properly)
  if (!mt::Valid(plugin, type, idx)) {
    spdlog::error("Failed to load plugin, or plugin is incomplete");
    mt::Clean(plugin, type, idx);
    render.integratorData.pop_back();
    plugin.integrators.pop_back();
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
std::string fileutil::FilePicker(std::string const & flags) {
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
