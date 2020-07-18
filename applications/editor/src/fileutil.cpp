#include "fileutil.hpp"

#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <mt-plugin-host/plugin.hpp>
#include <mt-plugin/plugin.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

namespace {

bool AttemptJsonStore(
  nlohmann::json const & info
, size_t & value, std::string const & label
) {
  if (auto s = info.find(label); s != info.end() && s->is_number_unsigned()) {
    value = s->get<size_t>();
    return true;
  }
  return false;
}

bool AttemptJsonStore(
  nlohmann::json const & info
, uint16_t & value, std::string const & label
) {
  if (auto s = info.find(label); s != info.end() && s->is_number_unsigned()) {
    value = s->get<uint16_t>();
    return true;
  }
  return false;
}

void LoadPluginIntegrator(
  mt::PluginInfoIntegrator & integrator
, mt::IntegratorData & data
, nlohmann::json const & info
) {
  if (auto s = info.find("state"); s != info.end() && s->is_string()) {
    data.renderingState = mt::ToRenderingState(s->get<std::string>().c_str());
  }

  if (auto s = info.find("aspect-ratio"); s != info.end() && s->is_string()) {
    data.imageAspectRatio = mt::ToAspectRatio(s->get<std::string>().c_str());
  }

  ::AttemptJsonStore(info, data.samplesPerPixel, "samples-per-pixel");
  ::AttemptJsonStore(info, data.pathsPerSample, "paths-per-sample");
  ::AttemptJsonStore(
    info, data.blockInternalIteratorMax, "iterations-per-block"
  );
  ::AttemptJsonStore(info, data.blockIteratorStride, "block-stride");
  ::AttemptJsonStore(info, data.imageResolution.x, "resolution");
  data.overrideImGuiImageResolution =
    ::AttemptJsonStore(info, data.imguiImageResolution, "imgui-resolution");

  mt::ApplyAspectRatioY(
    data.imageAspectRatio
  , data.imageResolution.x
  , data.imageResolution.y
  );
}

[[maybe_unused]]
void LoadPluginEmitter(nlohmann::json json) {
}

[[maybe_unused]]
void LoadPluginKernel(nlohmann::json json) {
}

[[maybe_unused]]
void LoadPluginMaterial(nlohmann::json json) {
}

[[maybe_unused]]
void LoadPluginCamera(nlohmann::json json) {
}

[[maybe_unused]]
void LoadPluginRandom(nlohmann::json json) {
}

[[maybe_unused]]
void LoadPluginUserInterface(nlohmann::json json) {
}
} // -- namespace

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

  for (auto const & info : json["plugins"]) {

    mt::PluginType pluginType;

    auto type = info.find("type");
    auto file = info.find("file");

    if (type == info.end()) {
      spdlog::error("Plugin needs to have a type");
      continue;
    }

    if (file == info.end()) {
      spdlog::error("Plugin needs to have a file");
      continue;
    }

    auto typeStr = type->get<std::string>();

    if (typeStr == "integrator") {
      pluginType = mt::PluginType::Integrator;
    } else if (typeStr == "kernel") {
      pluginType = mt::PluginType::Kernel;
    } else if (typeStr == "material") {
      pluginType = mt::PluginType::Material;
    } else if (typeStr == "camera") {
      pluginType = mt::PluginType::Camera;
    } else if (typeStr == "random") {
      pluginType = mt::PluginType::Random;
    } else if (typeStr == "userinterface") {
      pluginType = mt::PluginType::UserInterface;
    } else if (typeStr == "emitter") {
      pluginType = mt::PluginType::Emitter;
    } else if (typeStr == "dispatcher") {
      pluginType = mt::PluginType::Dispatcher;
    } else {
      spdlog::error("Unknown plugin type '{}' when loading config", typeStr);
      return;
    }

    if (
      !fileutil::LoadPlugin(
        plugin, render, file->get<std::string>(), pluginType
      )
    ) {
      continue;
    }

    // -- plugin loaded successfully here, can now set their configurations

    switch (pluginType) {
      default: break;
      case mt::PluginType::Integrator:
        ::LoadPluginIntegrator(
          plugin.integrators.back(),
          render.integratorData.back(),
          info
        );
      break;
    }
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
  if (type == mt::PluginType::Dispatcher) {
    plugin.dispatchers.emplace_back();
    idx = plugin.dispatchers.size()-1;
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
    if (type == mt::PluginType::Dispatcher) {
      plugin.dispatchers.pop_back();
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
