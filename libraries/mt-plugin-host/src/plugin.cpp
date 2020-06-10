#include <mt-plugin-host/plugin.hpp>

#include <mt-plugin/plugin.hpp>

#ifndef CR_HOST
#define CR_HOST
#endif
#include <cr/cr.h>

#include <array>

namespace {
  std::array<cr_plugin, static_cast<size_t>(mt::PluginType::Size)> plugins;
} // -- end namespace

uint32_t mt::LoadPlugin(
  mt::PluginInfo & pluginInfo
, PluginType pluginType, std::string const & file
, size_t idx
) {
  auto & ctx = ::plugins[static_cast<size_t>(pluginType)];

  // assign userdata to the respective pluginInfo parameter.
  switch (pluginType) {
    case PluginType::Integrator:
      ctx.userdata = reinterpret_cast<void*>(&pluginInfo.integrators[idx]);
    break;
    case PluginType::Kernel:
      ctx.userdata = reinterpret_cast<void*>(&pluginInfo.kernel);
    break;
    case PluginType::Material:
      ctx.userdata = reinterpret_cast<void*>(&pluginInfo.material);
    break;
    case PluginType::Camera:
      ctx.userdata = reinterpret_cast<void*>(&pluginInfo.camera);
    break;
    case PluginType::Random:
      ctx.userdata = reinterpret_cast<void*>(&pluginInfo.random);
    break;
    case PluginType::UserInterface:
      ctx.userdata = reinterpret_cast<void*>(&pluginInfo.userInterface);
    break;
    default: ctx.userdata = nullptr; break;
  }

  if (!cr_plugin_open(ctx, file.c_str())) { return CR_USER; }

  cr_set_temporary_path(ctx, "/tmp/");

  if (ctx.failure != CR_NONE) { return ctx.failure; }

  cr_plugin_update(ctx);

  return CR_NONE;
}

////////////////////////////////////////////////////////////////////////////////
void mt::FreePlugins() {
  for (auto & p : plugins) {
    p.userdata = nullptr; // want to make sure it doesn't try to free static mem
    if (p.p != nullptr) { cr_plugin_close(p); }
  }
}

////////////////////////////////////////////////////////////////////////////////
// checks if plugins need to be reloaded
void mt::UpdatePlugins() {
  for (auto & p : ::plugins) {
    if (p.userdata != nullptr) { cr_plugin_update(p); }
  }
}

//------------------------------------------------------------------------------
bool mt::Valid(
  mt::PluginInfo const & pluginInfo
, mt::PluginType pluginType
, size_t idx
) {
  switch (pluginType) {
    case mt::PluginType::Integrator:
      return
          pluginInfo.integrators[idx].Dispatch != nullptr
       && pluginInfo.integrators[idx].pluginType == pluginType
       && pluginInfo.integrators[idx].pluginLabel != nullptr
       ;
    case mt::PluginType::Kernel:
      return
          pluginInfo.kernel.Denoise != nullptr
       && pluginInfo.kernel.Tonemap != nullptr
       && pluginInfo.kernel.pluginType == pluginType
       && pluginInfo.kernel.pluginLabel != nullptr
      ;
    case mt::PluginType::Material:
      return
          pluginInfo.material.BsdfFs     != nullptr
       && pluginInfo.material.BsdfPdf    != nullptr
       && pluginInfo.material.BsdfSample != nullptr
       && pluginInfo.material.IsEmitter  != nullptr
       && pluginInfo.material.Load       != nullptr
       && pluginInfo.material.pluginType == pluginType
       && pluginInfo.material.pluginLabel != nullptr
      ;
    case mt::PluginType::Camera:
      return
          pluginInfo.camera.Dispatch != nullptr
       && pluginInfo.camera.pluginType == pluginType
       && pluginInfo.camera.pluginLabel != nullptr
       ;
    case mt::PluginType::Random:
      return
          pluginInfo.random.Clean != nullptr
       && pluginInfo.random.Initialize != nullptr
       && pluginInfo.random.SampleUniform1 != nullptr
       && pluginInfo.random.SampleUniform2 != nullptr
       && pluginInfo.random.SampleUniform3 != nullptr
       && pluginInfo.random.pluginType == pluginType
       && pluginInfo.random.pluginLabel != nullptr
      ;
    case mt::PluginType::UserInterface:
      return
          pluginInfo.userInterface.Dispatch != nullptr
       && pluginInfo.userInterface.pluginType == pluginType
       && pluginInfo.userInterface.pluginLabel != nullptr
      ;
    default: return false;
  }
}

//------------------------------------------------------------------------------
void mt::Clean(
  mt::PluginInfo & pluginInfo
, mt::PluginType pluginType
, size_t idx
) {
  // cleanup the plugin since it is invalid
  switch (pluginType) {
    case mt::PluginType::Integrator:
      pluginInfo.integrators[idx].Dispatch = nullptr;
      pluginInfo.integrators[idx].UiUpdate = nullptr;
      pluginInfo.integrators[idx].pluginType = mt::PluginType::Invalid;
      pluginInfo.integrators[idx].pluginLabel = nullptr;
    break;
    case mt::PluginType::Kernel:
      pluginInfo.kernel.Denoise = nullptr;
      pluginInfo.kernel.Tonemap = nullptr;
      pluginInfo.kernel.UiUpdate = nullptr;
      pluginInfo.kernel.pluginType = mt::PluginType::Invalid;
      pluginInfo.kernel.pluginLabel = nullptr;
    break;
    case mt::PluginType::Material:
      pluginInfo.material.BsdfFs     = nullptr;
      pluginInfo.material.BsdfPdf    = nullptr;
      pluginInfo.material.BsdfSample = nullptr;
      pluginInfo.material.IsEmitter  = nullptr;
      pluginInfo.material.Load       = nullptr;
      pluginInfo.material.UiUpdate   = nullptr;
      pluginInfo.material.pluginType = mt::PluginType::Invalid;
      pluginInfo.material.pluginLabel = nullptr;
    break;
    case mt::PluginType::Camera:
      pluginInfo.camera.Dispatch = nullptr;
      pluginInfo.camera.pluginType = mt::PluginType::Invalid;
      pluginInfo.camera.pluginLabel = nullptr;
    break;
    case mt::PluginType::Random:
      if (mt::Valid(pluginInfo, pluginType)) {
        pluginInfo.random.Clean();
      }
      pluginInfo.random.Clean = nullptr;
      pluginInfo.random.Initialize = nullptr;
      pluginInfo.random.SampleUniform1 = nullptr;
      pluginInfo.random.SampleUniform2 = nullptr;
      pluginInfo.random.SampleUniform3 = nullptr;
      pluginInfo.random.UiUpdate       = nullptr;
      pluginInfo.random.pluginType = mt::PluginType::Invalid;
      pluginInfo.random.pluginLabel = nullptr;
    break;
    case mt::PluginType::UserInterface:
      pluginInfo.userInterface.Dispatch = nullptr;
      pluginInfo.userInterface.pluginType = mt::PluginType::Invalid;
      pluginInfo.userInterface.pluginLabel = nullptr;
    break;
    default: return;
  }
}

//------------------------------------------------------------------------------
char const * ToString(mt::PluginType pluginType) {
  switch (pluginType) {
    case mt::PluginType::Integrator:    return "Integrator";
    case mt::PluginType::Kernel:        return "Kernel";
    case mt::PluginType::Material:      return "Material";
    case mt::PluginType::Camera:        return "Camera";
    case mt::PluginType::Random:        return "Random";
    case mt::PluginType::UserInterface: return "UserInterface";
    default: return "N/A";
  }
}
