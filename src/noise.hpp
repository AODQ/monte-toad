#pragma once

#include <glm/glm.hpp>

#include <random>
#include <variant>
#include <vector>

enum class NoiseType {
  White
, WhiteCached
, Blue
, Regular
};

template <NoiseType noiseType> struct Noise;

// --- white noise

template <> struct Noise<NoiseType::White> {
  Noise() = default;

  std::mt19937 rng;
  std::uniform_real_distribution<float> dist;

  static Noise<NoiseType::White> Construct();
};

float     SampleUniform1(Noise<NoiseType::White> & noise);
glm::vec2 SampleUniform2(Noise<NoiseType::White> & noise);

// --- white noise (cached, similar to how blue/regular noise works)

template <> struct Noise<NoiseType::WhiteCached> {
  Noise() = default;

  std::vector<glm::vec2> samples;

  std::mt19937 rng;
  std::uniform_int_distribution<uint32_t> dist;

  static Noise<NoiseType::WhiteCached> Construct(size_t samples);
};

float     SampleUniform1(Noise<NoiseType::WhiteCached> & noise);
glm::vec2 SampleUniform2(Noise<NoiseType::WhiteCached> & noise);

// --- regular noise

template <> struct Noise<NoiseType::Regular> {
  Noise() = default;

  size_t sides, it;

  static Noise<NoiseType::Regular> Construct(size_t samples = 32u);
};

float     SampleUniform1(Noise<NoiseType::Regular> & noise);
glm::vec2 SampleUniform2(Noise<NoiseType::Regular> & noise);

// --- blue noise

template <> struct Noise<NoiseType::Blue> {
  Noise() = default;

  std::vector<glm::vec2> samples;

  size_t it = 0;

  static Noise<NoiseType::Blue> Construct(size_t samples = 256u);
};

float     SampleUniform1(Noise<NoiseType::Blue> & noise);
glm::vec2 SampleUniform2(Noise<NoiseType::Blue> & noise);

using GenericNoiseGenerator =
  std::variant<
    Noise<NoiseType::White>
  , Noise<NoiseType::Blue>
  , Noise<NoiseType::Regular>
  >;

float     SampleUniform1(GenericNoiseGenerator & noise);
glm::vec2 SampleUniform2(GenericNoiseGenerator & noise);
