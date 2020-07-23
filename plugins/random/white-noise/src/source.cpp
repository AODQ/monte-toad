// white noise

#include <monte-toad/math.hpp>
#include <mt-plugin/enums.hpp>

#include <random>

namespace {
  std::mt19937 rng = std::mt19937(std::random_device()());
  std::uniform_real_distribution<float> distribution =
    std::uniform_real_distribution<float>(0.0f, 1.0f);
} // -- namespace

extern "C" {

char const * PluginLabel() { return "white noise"; }
mt::PluginType PluginType() { return mt::PluginType::Random; }

void Initialize() {}
void Clean() {}

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

} // -- end extern "C"
