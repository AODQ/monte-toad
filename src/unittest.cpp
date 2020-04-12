#include "io.hpp"
#include "noise.hpp"
#include "log.hpp"

#include <vector>
#include <variant>

void Test() {

  // -- test noises
  size_t constexpr samples = 512u*1u;
  using NoiseTuple = std::tuple<GenericNoiseGenerator, std::string>;
  for (auto & noisePair : std::vector<NoiseTuple> {
    NoiseTuple(Noise<NoiseType::White>::Construct(),          "whitenoise")
  , NoiseTuple(Noise<NoiseType::Regular>::Construct(samples), "regularnoise")
  , NoiseTuple(Noise<NoiseType::Blue>::Construct(samples),    "bluenoise")
  }) {
    auto buffer = Buffer::Construct(1024u, 1024u);
    auto & noise = std::get<0u>(noisePair);
    auto & noiseStr = std::get<1u>(noisePair);
    for (size_t i = 0u; i < samples; ++ i) {
      glm::uvec2 coord =
        glm::uvec2(
          SampleUniform2(noise) * glm::vec2(buffer.Width(), buffer.Height())
        );
      buffer.At(coord.x, coord.y) = glm::vec3(1.0f);
    }
    SaveImage(buffer, "unittest-" + noiseStr + ".ppm", true);
  }
}
