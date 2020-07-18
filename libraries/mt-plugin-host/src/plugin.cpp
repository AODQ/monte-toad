#include <mt-plugin-host/plugin.hpp>

#include <mt-plugin/plugin.hpp>

#include <array>
#include <memory>

#ifdef __unix__
#include <dlfcn.h>
#endif

namespace {

struct Plugin {
  Plugin(char const * filename);
  ~Plugin();
  void * data = nullptr;

  enum class Optional { Yes, No };

  struct FnCache {
    void * fnPtr;
    std::string fnStr;
    Optional optional;
  };

  std::vector<FnCache> functionCache;
  std::string filename;

  template <typename T> void LoadFunction(
    T & fn,
    char const * label,
    Optional optional = Optional::No
  );

  void Reload();
};

Plugin::Plugin(char const * filenameTmp) {
  this->filename = std::string{filenameTmp};
  this->data = dlopen(this->filename.c_str(), RTLD_NOW);
}

Plugin::~Plugin() {
  if (!this->data) { return; }
  dlclose(this->data);
  this->data = nullptr;
}

template <typename T> void Plugin::LoadFunction(
  T & fn,
  char const * label,
  Plugin::Optional optional
) {
  fn = reinterpret_cast<T>(dlsym(this->data, label));
  if (!fn && optional == Optional::No)
    { printf("Failed to load function '%s'; '%s'\n", label, dlerror()); }
}

void Plugin::Reload() {
  // attempt close
  if (this->data) { dlclose(this->data); }
  // open dll again
  this->data = dlopen(this->filename.c_str(), RTLD_NOW);

  // load cache
  for (auto & fn : functionCache) {
    fn.fnPtr = dlsym(this->data, fn.fnStr.c_str());
    if (!fn.fnPtr && fn.optional == Optional::No) {
      printf(
        "Failed to load function '%s'; '%s'\n"
      , fn.fnStr.c_str()
      , dlerror()
      );
    }
  }
}

struct Plugins {
  std::string filename;
  std::unique_ptr<Plugin> plugin;
};
std::vector<Plugins> plugins;

} // -- end namespace

bool mt::LoadPlugin(
  mt::PluginInfo & pluginInfo
, PluginType pluginType, std::string const & file
, size_t idx
) {

  // first find if the plugin has already been loaded, if that's the case then
  // error
  for (auto & plugin : plugins)
    { if (plugin.filename == file) { return false; } }

  // -- load plugin
  ::plugins.emplace_back(file, std::make_unique<Plugin>(file.c_str()));
  auto & plugin = ::plugins.back();
  auto & ctx = *plugin.plugin;

  // check plugin loaded
  if (!ctx.data) {
    ::plugins.pop_back();
    return false;
  }

  // -- load functions to respective plugin type
  switch (pluginType) {
    case PluginType::Integrator: {
      auto & unit = pluginInfo.integrators[idx];
      ctx.LoadFunction(unit.Dispatch, "Dispatch");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.RealTime, "RealTime");
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case PluginType::Kernel: {
      auto & unit = pluginInfo.kernel;
      ctx.LoadFunction(unit.Tonemap, "Tonemap");
      ctx.LoadFunction(unit.Denoise, "Denoise");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case PluginType::Material: {
      auto & unit = pluginInfo.material;
      ctx.LoadFunction(unit.Load, "Load");
      ctx.LoadFunction(unit.BsdfSample, "BsdfSample");
      ctx.LoadFunction(unit.BsdfFs, "BsdfFs");
      ctx.LoadFunction(unit.BsdfPdf, "BsdfPdf");
      ctx.LoadFunction(unit.IsEmitter, "IsEmitter");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case PluginType::Camera: {
      auto & unit = pluginInfo.camera;
      ctx.LoadFunction(unit.Dispatch, "Dispatch");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case PluginType::Random: {
      auto & unit = pluginInfo.random;
      ctx.LoadFunction(unit.Initialize, "Initialize");
      ctx.LoadFunction(unit.Clean, "Clean");
      ctx.LoadFunction(unit.SampleUniform1, "SampleUniform1");
      ctx.LoadFunction(unit.SampleUniform2, "SampleUniform2");
      ctx.LoadFunction(unit.SampleUniform3, "SampleUniform3");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case PluginType::UserInterface: {
      auto & unit = pluginInfo.userInterface;
      ctx.LoadFunction(unit.Dispatch, "Dispatch");
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case PluginType::Emitter: {
      auto & unit = pluginInfo.emitters[idx];
      ctx.LoadFunction(unit.IsSkybox, "IsSkybox");
      ctx.LoadFunction(unit.SampleLi, "SampleLi");
      ctx.LoadFunction(unit.SampleWo, "SampleWo");
      ctx.LoadFunction(unit.Precompute, "Precompute");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case PluginType::Dispatcher: {
      auto & unit = pluginInfo.dispatchers[idx];
      ctx.LoadFunction(unit.DispatchBlockRegion, "DispatchBlockRegion");
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    default: break;
  }

  return true;
}

void mt::FreePlugin(size_t idx) {
  ::plugins.erase(::plugins.begin() + idx);
}

////////////////////////////////////////////////////////////////////////////////
void mt::FreePlugins() {
  ::plugins.clear();
}

////////////////////////////////////////////////////////////////////////////////
// checks if plugins need to be reloaded TODO
void mt::UpdatePlugins() {
  for (auto & plugin : ::plugins) {
    plugin.plugin->Reload();
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
          idx < pluginInfo.integrators.size()
       && pluginInfo.integrators[idx].Dispatch != nullptr
       && pluginInfo.integrators[idx].PluginType != nullptr
       && pluginInfo.integrators[idx].PluginType() == pluginType
       && pluginInfo.integrators[idx].PluginLabel != nullptr
       && pluginInfo.integrators[idx].RealTime != nullptr
      ;
    case mt::PluginType::Kernel:
      return
          pluginInfo.kernel.Denoise != nullptr
       && pluginInfo.kernel.Tonemap != nullptr
       && pluginInfo.kernel.PluginType() == pluginType
       && pluginInfo.kernel.PluginLabel != nullptr
      ;
    case mt::PluginType::Material:
      return
          pluginInfo.material.BsdfFs     != nullptr
       && pluginInfo.material.BsdfPdf    != nullptr
       && pluginInfo.material.BsdfSample != nullptr
       && pluginInfo.material.IsEmitter  != nullptr
       && pluginInfo.material.Load       != nullptr
       && pluginInfo.material.PluginType != nullptr
       && pluginInfo.material.PluginType() == pluginType
       && pluginInfo.material.PluginLabel != nullptr
      ;
    case mt::PluginType::Camera:
      return
          pluginInfo.camera.Dispatch != nullptr
       && pluginInfo.camera.PluginType != nullptr
       && pluginInfo.camera.PluginType() == pluginType
       && pluginInfo.camera.PluginLabel != nullptr
       ;
    case mt::PluginType::Random:
      return
          pluginInfo.random.Clean != nullptr
       && pluginInfo.random.Initialize != nullptr
       && pluginInfo.random.SampleUniform1 != nullptr
       && pluginInfo.random.SampleUniform2 != nullptr
       && pluginInfo.random.SampleUniform3 != nullptr
       && pluginInfo.random.PluginType != nullptr
       && pluginInfo.random.PluginType() == pluginType
       && pluginInfo.random.PluginLabel != nullptr
      ;
    case mt::PluginType::UserInterface:
      return
          pluginInfo.userInterface.Dispatch != nullptr
       && pluginInfo.userInterface.PluginType != nullptr
       && pluginInfo.userInterface.PluginType() == pluginType
       && pluginInfo.userInterface.PluginLabel != nullptr
      ;
    case mt::PluginType::Emitter:
      return
          idx < pluginInfo.emitters.size()
       && pluginInfo.emitters[idx].SampleLi != nullptr
       && pluginInfo.emitters[idx].SampleWo != nullptr
       && pluginInfo.emitters[idx].Precompute != nullptr
       && pluginInfo.emitters[idx].PluginType != nullptr
       && pluginInfo.emitters[idx].PluginType() == pluginType
       && pluginInfo.emitters[idx].PluginLabel != nullptr
      ;
    case mt::PluginType::Dispatcher:
      return
          idx < pluginInfo.dispatchers.size()
       && pluginInfo.dispatchers[idx].DispatchBlockRegion != nullptr
       && pluginInfo.dispatchers[idx].PluginType != nullptr
       && pluginInfo.dispatchers[idx].PluginType() == pluginType
       && pluginInfo.dispatchers[idx].PluginLabel != nullptr
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
      pluginInfo.integrators[idx].RealTime = nullptr;
      pluginInfo.integrators[idx].PluginType = nullptr;
      pluginInfo.integrators[idx].PluginLabel = nullptr;
    break;
    case mt::PluginType::Kernel:
      pluginInfo.kernel.Denoise = nullptr;
      pluginInfo.kernel.Tonemap = nullptr;
      pluginInfo.kernel.UiUpdate = nullptr;
      pluginInfo.kernel.PluginType = nullptr;
      pluginInfo.kernel.PluginLabel = nullptr;
    break;
    case mt::PluginType::Material:
      pluginInfo.material.BsdfFs      = nullptr;
      pluginInfo.material.BsdfPdf     = nullptr;
      pluginInfo.material.BsdfSample  = nullptr;
      pluginInfo.material.IsEmitter   = nullptr;
      pluginInfo.material.Load        = nullptr;
      pluginInfo.material.UiUpdate    = nullptr;
      pluginInfo.material.PluginType  = nullptr;
      pluginInfo.material.PluginLabel = nullptr;

      if (pluginInfo.material.userdata)
        { free(pluginInfo.material.userdata); }
      pluginInfo.material.userdata    = nullptr;
    break;
    case mt::PluginType::Camera:
      pluginInfo.camera.Dispatch = nullptr;
      pluginInfo.camera.PluginType = nullptr;
      pluginInfo.camera.PluginLabel = nullptr;
      pluginInfo.camera.UiUpdate = nullptr;
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
      pluginInfo.random.PluginType = nullptr;
      pluginInfo.random.PluginLabel = nullptr;
    break;
    case mt::PluginType::UserInterface:
      pluginInfo.userInterface.Dispatch = nullptr;
      pluginInfo.userInterface.PluginType = nullptr;
      pluginInfo.userInterface.PluginLabel = nullptr;
    break;
    case mt::PluginType::Emitter:
      pluginInfo.emitters[idx].SampleLi = nullptr;
      pluginInfo.emitters[idx].SampleWo = nullptr;
      pluginInfo.emitters[idx].Precompute = nullptr;
      pluginInfo.emitters[idx].UiUpdate = nullptr;
      pluginInfo.emitters[idx].PluginType = nullptr;
      pluginInfo.emitters[idx].PluginLabel = nullptr;
      pluginInfo.emitters[idx].IsSkybox = nullptr;
    break;
    case mt::PluginType::Dispatcher:
      pluginInfo.dispatchers[idx].DispatchBlockRegion = nullptr;
      pluginInfo.dispatchers[idx].PluginType = nullptr;
      pluginInfo.dispatchers[idx].PluginLabel = nullptr;
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
    case mt::PluginType::Emitter:       return "Emitter";
    case mt::PluginType::Dispatcher:    return "Dispatcher";
    default: return "N/A";
  }
}
