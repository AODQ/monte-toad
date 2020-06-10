#include <mt-plugin/plugin.hpp>

#include <cr/cr.h>

#include <random>

namespace {
  void Clean() {}
  void Load() {}

  std::mt19937 rng = std::mt19937(std::random_device()());
  std::uniform_real_distribution<float> distribution =
    std::uniform_real_distribution<float>(0.0f, 1.0f);

  float SampleUniform1() { return ::distribution(::rng); }

  glm::vec2 SampleUniform2() {
    return glm::vec2(::distribution(::rng), ::distribution(::rng));
  }

  glm::vec3 SampleUniform3() {
    return
      glm::vec3(
        ::distribution(::rng),
        ::distribution(::rng),
        ::distribution(::rng)
      );
  }
}

CR_EXPORT int cr_main(struct cr_plugin * ctx, enum cr_op operation) {
  // return immediately if an update, this won't do anything
  if (operation == CR_UNLOAD || operation == CR_STEP) { return 0; }
  if (!ctx || !ctx->userdata) { return 0; }

  auto & random =
    *reinterpret_cast<mt::PluginInfoRandom*>(ctx->userdata);

  switch (operation) {
    case CR_STEP: break;
    case CR_LOAD:
      random.Clean = &::Clean;
      random.Initialize = &::Load;
      random.SampleUniform1 = &::SampleUniform1;
      random.SampleUniform2 = &::SampleUniform2;
      random.SampleUniform3 = &::SampleUniform3;
      random.pluginType = mt::PluginType::Random;
      random.pluginLabel = "uniform noise";
      random.UiUpdate = nullptr;
    break;
    case CR_UNLOAD: break;
    case CR_CLOSE: break;
  }

  return 0;
}
