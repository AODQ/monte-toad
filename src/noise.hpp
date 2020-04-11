#pragma once

#include <glm/glm.hpp>

#include <random>
#include <vector>

enum class NoiseType {
  White
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

// --- regular noise

template <> struct Noise<NoiseType::Regular> {
  Noise() = default;

  size_t sides, it;

  static Noise<NoiseType::Regular> Construct(size_t samples = 128u);
};

float     SampleUniform1(Noise<NoiseType::Regular> & noise);
glm::vec2 SampleUniform2(Noise<NoiseType::Regular> & noise);

// --- blue noise

struct PoissonGrid {
  PoissonGrid() = default;

  uint32_t dimensions;
  float minDistance;
  float cellSize;

  // each index refers to a point in points list
  std::vector<std::vector<size_t>> indices;
  std::vector<glm::vec2> points;

  static PoissonGrid Construct(float minDistance);
};

template <> struct Noise<NoiseType::Blue> {
  Noise() = default;

  PoissonGrid grid;

  std::mt19937 rng;
  std::uniform_real_distribution<float> dist;

  std::vector<glm::vec2> activeSamples;
  glm::vec2 nextSample;

  size_t numSamples;
  size_t samplesLeft;
  float minDistance;

  size_t samplesLimit = 30;

  static Noise<NoiseType::Blue> Construct(size_t samples = 128u);
};

float     SampleUniform1(Noise<NoiseType::Blue> & noise);
glm::vec2 SampleUniform2(Noise<NoiseType::Blue> & noise);
