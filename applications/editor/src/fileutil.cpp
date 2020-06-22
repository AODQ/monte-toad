#include "fileutil.hpp"

#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <mt-plugin-host/plugin.hpp>
#include <mt-plugin/plugin.hpp>

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
    } else if (info[0] == "emitter") {
      type = mt::PluginType::Emitter;
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
  if (type == mt::PluginType::Emitter) {
    plugin.emitters.emplace_back();
    idx = plugin.emitters.size()-1;
  }

  mt::Clean(plugin, type, idx);

  mt::LoadPlugin(plugin, type, file, idx);

  // check that the plugin loaded itself properly (ei members set properly)
  if (!mt::Valid(plugin, type, idx)) {
    spdlog::error("Failed to load plugin {}, or plugin is incomplete", file);
    mt::Clean(plugin, type, idx);

    if (type == mt::PluginType::Integrator) {
      render.integratorData.pop_back();
      plugin.integrators.pop_back();
    }
    if (type == mt::PluginType::Emitter) {
      plugin.emitters.pop_back();
    }

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
