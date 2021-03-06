#include <mt-plugin-host/plugin.hpp>

#include <mt-plugin/plugin.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
  #include <spdlog/spdlog.h>
#pragma GCC diagnostic pop

#include <memory>
#include <vector>

#ifdef __unix__
#include <dlfcn.h>
#endif

namespace {

struct Plugin {
  // -- constructors
  Plugin(char const * filename, mt::PluginType type, size_t idx);
  ~Plugin();

  // -- members
  std::string filename;
  void * data = nullptr;
  mt::PluginType type;
  size_t idx;

  // -- static
  enum class Optional { Yes, No };

  // -- functions
  template <typename T> void LoadFunction(
    T & fn,
    char const * label,
    Optional optional = Optional::No
  );

  void Reload();
};

Plugin::Plugin(char const * filename_, mt::PluginType type_, size_t idx_)
  : filename(filename_), type(type_), idx(idx_)
{
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
    { spdlog::error("Failed to load function '{}'; '{}'", label, dlerror()); }

  spdlog::debug("Loaded function {}\n", reinterpret_cast<void*>(fn));
}

void Plugin::Reload() {
  spdlog::debug("Reloading plugin '{}'", this->filename.c_str());
  // attempt close
  if (this->data) { dlclose(this->data); }
  // open dll again
  this->data = dlopen(this->filename.c_str(), RTLD_NOW);
}

std::vector<std::unique_ptr<Plugin>> plugins;

void LoadPluginFunctions(mt::PluginInfo & plugin , Plugin & ctx) {
  switch (ctx.type) {
    case mt::PluginType::Integrator: {
      auto & unit = plugin.integrators[ctx.idx];
      ctx.LoadFunction(unit.Dispatch, "Dispatch", Plugin::Optional::Yes);
      ctx.LoadFunction(
        unit.DispatchRealtime, "DispatchRealtime", Plugin::Optional::Yes
      );
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.RealTime, "RealTime");
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case mt::PluginType::AccelerationStructure: {
      auto & unit = plugin.accelerationStructure;
      ctx.LoadFunction(unit.Construct, "Construct");
      ctx.LoadFunction(unit.IntersectClosest, "IntersectClosest");
      ctx.LoadFunction(unit.GetTriangle, "GetTriangle");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate");
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case mt::PluginType::Kernel: {
      auto & unit = plugin.kernels[ctx.idx];
      ctx.LoadFunction(unit.ApplyKernel, "ApplyKernel");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case mt::PluginType::Bsdf: {
      auto & unit = plugin.bsdfs[ctx.idx];
      ctx.LoadFunction(unit.Allocate, "Allocate");
      ctx.LoadFunction(unit.BsdfSample, "BsdfSample");
      ctx.LoadFunction(unit.BsdfFs, "BsdfFs");
      ctx.LoadFunction(unit.BsdfPdf, "BsdfPdf");
      ctx.LoadFunction(unit.AlbedoApproximation, "AlbedoApproximation");
      ctx.LoadFunction(unit.IsEmitter, "IsEmitter");
      ctx.LoadFunction(unit.BsdfType, "BsdfType");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case mt::PluginType::Material:{
      auto & unit = plugin.material;
      ctx.LoadFunction(unit.Allocate, "Allocate");
      ctx.LoadFunction(unit.IsEmitter, "IsEmitter");
      ctx.LoadFunction(unit.Sample, "Sample");
      ctx.LoadFunction(unit.Pdf, "Pdf");
      ctx.LoadFunction(unit.IndirectPdf, "IndirectPdf");
      ctx.LoadFunction(unit.EmitterFs, "EmitterFs");
      ctx.LoadFunction(unit.BsdfFs, "BsdfFs");
      ctx.LoadFunction(unit.AlbedoApproximation, "AlbedoApproximation");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate");
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case mt::PluginType::Camera: {
      auto & unit = plugin.camera;
      ctx.LoadFunction(unit.Dispatch, "Dispatch");
      ctx.LoadFunction(
        unit.WorldCoordToUv, "WorldCoordToUv", Plugin::Optional::Yes
      );
      ctx.LoadFunction(
        unit.UpdateCamera, "UpdateCamera", Plugin::Optional::Yes
      );
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case mt::PluginType::Random: {
      auto & unit = plugin.random;
      ctx.LoadFunction(unit.Initialize, "Initialize");
      ctx.LoadFunction(unit.Clean, "Clean");
      ctx.LoadFunction(unit.SampleUniform1, "SampleUniform1");
      ctx.LoadFunction(unit.SampleUniform2, "SampleUniform2");
      ctx.LoadFunction(unit.SampleUniform3, "SampleUniform3");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case mt::PluginType::UserInterface: {
      auto & unit = plugin.userInterface;
      ctx.LoadFunction(unit.Dispatch, "Dispatch");
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case mt::PluginType::Emitter: {
      auto & unit = plugin.emitters[ctx.idx];
      ctx.LoadFunction(unit.IsSkybox, "IsSkybox");
      ctx.LoadFunction(unit.SampleLi, "SampleLi");
      ctx.LoadFunction(unit.SampleWo, "SampleWo");
      ctx.LoadFunction(unit.Precompute, "Precompute");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate", Plugin::Optional::Yes);
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    case mt::PluginType::Dispatcher: {
      auto & unit = plugin.dispatchers[ctx.idx];
      ctx.LoadFunction(unit.DispatchRender, "DispatchRender");
      ctx.LoadFunction(unit.UiUpdate, "UiUpdate");
      ctx.LoadFunction(unit.PluginType, "PluginType");
      ctx.LoadFunction(unit.PluginLabel, "PluginLabel");
    } break;
    default: break;
  }
}

} // -- namespace

bool mt::LoadPlugin(
  mt::PluginInfo & plugin
, PluginType pluginType, std::string const & file
, size_t idx
) {

  // first find if the plugin has already been loaded, if that's the case then
  // error
  for (auto & pluginIt : plugins) {
    if (pluginIt->filename == file) {
      spdlog::error("Plugin '{}' already loaded", pluginIt->filename);
      return false;
    }
  }

  // -- load plugin
  ::plugins.emplace_back(
    std::make_unique<Plugin>(file.c_str(), pluginType, idx)
  );
  auto & pluginEnd = ::plugins.back();

  // check plugin loaded
  if (!pluginEnd->data) {
    ::plugins.pop_back();
    spdlog::error("shared object file could not load correctly");
    return false;
  }

  // -- load functions to respective plugin type
  LoadPluginFunctions(plugin, *pluginEnd);

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
void mt::UpdatePlugins(
  mt::PluginInfo & plugin
) {
  for (auto & pluginIt : ::plugins) {
    pluginIt->Reload();

    // -- load functions to respective plugin type
    LoadPluginFunctions(plugin, *pluginIt);
  }
}

//------------------------------------------------------------------------------
bool mt::Valid(
  mt::PluginInfo const & plugin
, mt::PluginType pluginType
, size_t idx
) {
  switch (pluginType) {
    case mt::PluginType::Integrator:
      return
          idx < plugin.integrators.size()
       && (
            plugin.integrators[idx].Dispatch != nullptr
         || plugin.integrators[idx].DispatchRealtime != nullptr
          )
       && plugin.integrators[idx].PluginType != nullptr
       && plugin.integrators[idx].PluginType() == pluginType
       && plugin.integrators[idx].PluginLabel != nullptr
       && plugin.integrators[idx].RealTime != nullptr
      ;
    case mt::PluginType::AccelerationStructure:
      return
          plugin.accelerationStructure.Construct != nullptr
       && plugin.accelerationStructure.IntersectClosest != nullptr
       && plugin.accelerationStructure.GetTriangle != nullptr
       && plugin.accelerationStructure.PluginType != nullptr
       && plugin.accelerationStructure.PluginType() == pluginType
       && plugin.accelerationStructure.PluginLabel != nullptr
      ;
    case mt::PluginType::Kernel:
      return
          plugin.kernels[idx].ApplyKernel != nullptr
       && plugin.kernels[idx].PluginType() == pluginType
       && plugin.kernels[idx].PluginLabel != nullptr
      ;
    case mt::PluginType::Bsdf:
      return
          idx < plugin.bsdfs.size()
       && plugin.bsdfs[idx].BsdfFs != nullptr
       && plugin.bsdfs[idx].BsdfPdf  != nullptr
       && plugin.bsdfs[idx].AlbedoApproximation != nullptr
       && plugin.bsdfs[idx].BsdfSample != nullptr
       && plugin.bsdfs[idx].IsEmitter != nullptr
       && plugin.bsdfs[idx].BsdfType != nullptr
       && plugin.bsdfs[idx].Allocate != nullptr
       && plugin.bsdfs[idx].PluginType != nullptr
       && plugin.bsdfs[idx].PluginType() == pluginType
       && plugin.bsdfs[idx].PluginLabel != nullptr
      ;
    case mt::PluginType::Material:
      return
          plugin.material.Allocate != nullptr
       && plugin.material.IsEmitter != nullptr
       && plugin.material.Sample != nullptr
       && plugin.material.Pdf != nullptr
       && plugin.material.IndirectPdf != nullptr
       && plugin.material.EmitterFs != nullptr
       && plugin.material.BsdfFs != nullptr
       && plugin.material.AlbedoApproximation != nullptr
       && plugin.material.PluginType != nullptr
       && plugin.material.PluginType() == pluginType
       && plugin.material.PluginLabel != nullptr
      ;
    case mt::PluginType::Camera:
      return
          plugin.camera.Dispatch != nullptr
       && plugin.camera.PluginType != nullptr
       && plugin.camera.PluginType() == pluginType
       && plugin.camera.PluginLabel != nullptr
       ;
    case mt::PluginType::Random:
      return
          plugin.random.Clean != nullptr
       && plugin.random.Initialize != nullptr
       && plugin.random.SampleUniform1 != nullptr
       && plugin.random.SampleUniform2 != nullptr
       && plugin.random.SampleUniform3 != nullptr
       && plugin.random.PluginType != nullptr
       && plugin.random.PluginType() == pluginType
       && plugin.random.PluginLabel != nullptr
      ;
    case mt::PluginType::UserInterface:
      return
          plugin.userInterface.Dispatch != nullptr
       && plugin.userInterface.PluginType != nullptr
       && plugin.userInterface.PluginType() == pluginType
       && plugin.userInterface.PluginLabel != nullptr
      ;
    case mt::PluginType::Emitter:
      return
          idx < plugin.emitters.size()
       && plugin.emitters[idx].SampleLi != nullptr
       && plugin.emitters[idx].SampleWo != nullptr
       && plugin.emitters[idx].Precompute != nullptr
       && plugin.emitters[idx].PluginType != nullptr
       && plugin.emitters[idx].PluginType() == pluginType
       && plugin.emitters[idx].PluginLabel != nullptr
      ;
    case mt::PluginType::Dispatcher:
      return
          idx < plugin.dispatchers.size()
       && plugin.dispatchers[idx].DispatchRender != nullptr
       && plugin.dispatchers[idx].PluginType != nullptr
       && plugin.dispatchers[idx].PluginType() == pluginType
       && plugin.dispatchers[idx].PluginLabel != nullptr
      ;
    default: return false;
  }
}

//------------------------------------------------------------------------------
void mt::Clean(
  mt::PluginInfo & plugin
, mt::PluginType pluginType
, size_t idx
) {
  // cleanup the plugin since it is invalid
  switch (pluginType) {
    case mt::PluginType::Integrator:
      plugin.integrators[idx].Dispatch = nullptr;
      plugin.integrators[idx].DispatchRealtime = nullptr;
      plugin.integrators[idx].UiUpdate = nullptr;
      plugin.integrators[idx].RealTime = nullptr;
      plugin.integrators[idx].PluginType = nullptr;
      plugin.integrators[idx].PluginLabel = nullptr;
    break;
    case mt::PluginType::AccelerationStructure:
      plugin.accelerationStructure.Construct = nullptr;
      plugin.accelerationStructure.IntersectClosest = nullptr;
      plugin.accelerationStructure.GetTriangle = nullptr;
      plugin.accelerationStructure.UiUpdate = nullptr;
      plugin.accelerationStructure.PluginType = nullptr;
      plugin.accelerationStructure.PluginLabel = nullptr;
    break;
    case mt::PluginType::Kernel:
      plugin.kernels[idx].ApplyKernel = nullptr;
      plugin.kernels[idx].UiUpdate = nullptr;
      plugin.kernels[idx].PluginType = nullptr;
      plugin.kernels[idx].PluginLabel = nullptr;
    break;
    case mt::PluginType::Bsdf:
      plugin.bsdfs[idx].BsdfFs              = nullptr;
      plugin.bsdfs[idx].BsdfPdf             = nullptr;
      plugin.bsdfs[idx].AlbedoApproximation = nullptr;
      plugin.bsdfs[idx].BsdfSample          = nullptr;
      plugin.bsdfs[idx].IsEmitter           = nullptr;
      plugin.bsdfs[idx].BsdfType            = nullptr;
      plugin.bsdfs[idx].Allocate            = nullptr;
      plugin.bsdfs[idx].UiUpdate            = nullptr;
      plugin.bsdfs[idx].PluginType          = nullptr;
      plugin.bsdfs[idx].PluginLabel         = nullptr;
    break;
    case mt::PluginType::Material:
      plugin.material.Allocate            = nullptr;
      plugin.material.IsEmitter           = nullptr;
      plugin.material.Sample              = nullptr;
      plugin.material.Pdf                 = nullptr;
      plugin.material.IndirectPdf         = nullptr;
      plugin.material.EmitterFs           = nullptr;
      plugin.material.BsdfFs              = nullptr;
      plugin.material.AlbedoApproximation = nullptr;
      plugin.material.PluginType          = nullptr;
      plugin.material.PluginLabel         = nullptr;
    break;
    case mt::PluginType::Camera:
      plugin.camera.Dispatch = nullptr;
      plugin.camera.PluginType = nullptr;
      plugin.camera.PluginLabel = nullptr;
      plugin.camera.UiUpdate = nullptr;
    break;
    case mt::PluginType::Random:
      if (mt::Valid(plugin, pluginType)) {
        plugin.random.Clean();
      }
      plugin.random.Clean = nullptr;
      plugin.random.Initialize = nullptr;
      plugin.random.SampleUniform1 = nullptr;
      plugin.random.SampleUniform2 = nullptr;
      plugin.random.SampleUniform3 = nullptr;
      plugin.random.UiUpdate       = nullptr;
      plugin.random.PluginType = nullptr;
      plugin.random.PluginLabel = nullptr;
    break;
    case mt::PluginType::UserInterface:
      plugin.userInterface.Dispatch = nullptr;
      plugin.userInterface.PluginType = nullptr;
      plugin.userInterface.PluginLabel = nullptr;
    break;
    case mt::PluginType::Emitter:
      plugin.emitters[idx].SampleLi = nullptr;
      plugin.emitters[idx].SampleWo = nullptr;
      plugin.emitters[idx].Precompute = nullptr;
      plugin.emitters[idx].UiUpdate = nullptr;
      plugin.emitters[idx].PluginType = nullptr;
      plugin.emitters[idx].PluginLabel = nullptr;
      plugin.emitters[idx].IsSkybox = nullptr;
    break;
    case mt::PluginType::Dispatcher:
      plugin.dispatchers[idx].DispatchRender = nullptr;
      plugin.dispatchers[idx].UiUpdate = nullptr;
      plugin.dispatchers[idx].PluginType = nullptr;
      plugin.dispatchers[idx].PluginLabel = nullptr;
    break;
    default: return;
  }
}

//------------------------------------------------------------------------------
char const * ToString(mt::PluginType pluginType) {
  switch (pluginType) {
    case mt::PluginType::Integrator:            return "Integrator";
    case mt::PluginType::AccelerationStructure: return "AccelerationStructure";
    case mt::PluginType::Kernel:                return "Kernel";
    case mt::PluginType::Bsdf:                  return "Bsdf";
    case mt::PluginType::Material:              return "Material";
    case mt::PluginType::Camera:                return "Camera";
    case mt::PluginType::Random:                return "Random";
    case mt::PluginType::UserInterface:         return "UserInterface";
    case mt::PluginType::Emitter:               return "Emitter";
    case mt::PluginType::Dispatcher:            return "Dispatcher";
    default: return "N/A";
  }
}
