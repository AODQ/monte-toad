#include "noise.hpp"

#include "log.hpp"

constexpr static float Tau    = 6.283185307179586f;

////////////////////////////////////////////////////////////////////////////////
Noise<NoiseType::White> Noise<NoiseType::White>::Construct() {
  Noise<NoiseType::White> self;
  self.rng = std::mt19937(std::random_device()());
  self.dist = std::uniform_real_distribution<float>(0.0f, 1.0f);
  return self;
}

////////////////////////////////////////////////////////////////////////////////
float SampleUniform1(Noise<NoiseType::White> & self) {
  return self.dist(self.rng);
}

////////////////////////////////////////////////////////////////////////////////
glm::vec2 SampleUniform2(Noise<NoiseType::White> & self) {
  return glm::vec2(self.dist(self.rng), self.dist(self.rng));
}

////////////////////////////////////////////////////////////////////////////////
Noise<NoiseType::Regular> Noise<NoiseType::Regular>::Construct(size_t samples) {
  Noise<NoiseType::Regular> self;
  self.sides = static_cast<size_t>(glm::sqrt(samples));
  self.it = 0;
  return self;
}

////////////////////////////////////////////////////////////////////////////////
float SampleUniform1(Noise<NoiseType::Regular> & self) {
  return SampleUniform2(self).x;
}

////////////////////////////////////////////////////////////////////////////////
glm::vec2 SampleUniform2(Noise<NoiseType::Regular> & self) {
  self.it = (self.it + 1) % (self.sides*self.sides);
  return
    glm::vec2(
      (self.it%self.sides)/static_cast<float>(self.sides)
    , (self.it/self.sides)/static_cast<float>(self.sides)
    );
}

////////////////////////////////////////////////////////////////////////////////
float ToroidalDistance(glm::vec2 p0, glm::vec2 p1) {
  glm::vec2 p = glm::abs(p1 - p0);

  if (p.x > 0.5f) { p.x = 1.0f - p.x; }
  if (p.y > 0.5f) { p.y = 1.0f - p.y; }

  return glm::length(p);
}

////////////////////////////////////////////////////////////////////////////////
PoissonGrid PoissonGrid::Construct(float minDistance) {
  PoissonGrid grid;
  grid.minDistance = minDistance;
  grid.cellSize = minDistance / glm::sqrt(2.0f);
  grid.dimensions  = static_cast<size_t>(glm::ceil(1.0f / grid.cellSize));

  // create grid WxH where each element is not referring to any point (-1)
  grid.indices.resize(grid.dimensions);
  for (auto & xIndices : grid.indices) {
    xIndices.resize(grid.dimensions);
    for (auto & idx : xIndices) { idx = static_cast<size_t>(-1); }
  }

  return grid;
}

////////////////////////////////////////////////////////////////////////////////
// inserts point into grid
void Insert(PoissonGrid self, glm::vec2 const & p) {
  auto const gp = glm::uvec2(p.x/self.cellSize, p.y/self.cellSize);
  // to preserve correct index location, set index then push back point
  self.indices[gp.x][gp.y] = self.points.size();
  self.points.emplace_back(p);
}

////////////////////////////////////////////////////////////////////////////////
// Returns whether the given point has an adjacent distance to any point less
// than minDistance, which is necessary to check if a point is adequately far
// from existing samples
bool InRange(PoissonGrid self, glm::vec2 const & point) {
  auto const gp = glm::ivec2(point.x/self.cellSize, point.y/self.cellSize);
  int32_t const adjacentDist = 5;

  for (auto i = gp.x - adjacentDist; i < gp.x + adjacentDist; ++ i)
  for (auto j = gp.y - adjacentDist; j < gp.y + adjacentDist; ++ j) {
    // apply wrap-around to make the poisson distribution toroidal
    auto const idx = self.indices[i % self.dimensions][j % self.dimensions];
    if (idx == static_cast<size_t>(-1)) { continue; }

    glm::vec2 const testPoint = self.points[idx];

    glm::vec2 toroidalPoint = glm::abs(testPoint - point);
    if (toroidalPoint.x > 0.5f) { toroidalPoint.x = 0.5f - toroidalPoint.x; }
    if (toroidalPoint.y > 0.5f) { toroidalPoint.y = 0.5f - toroidalPoint.y; }

    if (ToroidalDistance(testPoint, point) < self.minDistance) { return true; }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
Noise<NoiseType::Blue> Noise<NoiseType::Blue>::Construct(size_t samples) {
  Noise<NoiseType::Blue> self;

  self.rng = std::mt19937(std::random_device()());
  self.dist = std::uniform_real_distribution<float>(0.0f, 1.0f);
  self.numSamples = samples;
  self.samplesLeft = samples-1;
  self.minDistance = glm::sqrt(samples)/static_cast<float>(samples);
  self.grid = PoissonGrid::Construct(self.minDistance);

  self.nextSample = glm::vec2(self.dist(self.rng), self.dist(self.rng));
  self.activeSamples.emplace_back(self.nextSample);
  Insert(self.grid, self.nextSample);

  return self;
}

////////////////////////////////////////////////////////////////////////////////
float SampleUniform1(Noise<NoiseType::Blue> & self) {
  return SampleUniform2(self).x;
}

////////////////////////////////////////////////////////////////////////////////
glm::vec2 SampleUniform2(Noise<NoiseType::Blue> & self) {
  while (true) {
    if (self.activeSamples.empty() || self.samplesLeft == 0)
      { return glm::vec2(0.0f); }

    glm::vec2 activePoint;
    size_t activeSampleIdx;
    { // pop a random point
      std::uniform_int_distribution<size_t> dist(
        0u, self.activeSamples.size()-1
      );
      activeSampleIdx = dist(self.rng);
      activePoint = self.activeSamples[activeSampleIdx];
    }

    // -- generate `sampleLimit` number of points around the chosen activePoint
    for (size_t i = 0; i < self.samplesLimit; ++ i) {
      glm::vec2 randomPoint;
      { // generate random point near active point
        float const
          radius = self.minDistance * (self.dist(self.rng) + 1.0f)
        , theta  = Tau * self.dist(self.rng);

        randomPoint =
          glm::vec2(
            activePoint.x + radius*glm::cos(theta)
          , activePoint.y + radius*glm::sin(theta)
          );

        // since this point could lie outside of range [0, 1], apply wrapping
        randomPoint = glm::mod(randomPoint, glm::vec2(1.0f));
      }

      // -- if point is in bounds & in range of grid, it is valid; add to list
      if (InRange(self.grid, randomPoint)) { continue; }

      self.activeSamples.emplace_back(randomPoint);
      Insert(self.grid, randomPoint);
      return randomPoint;
    }

    // failed to find a proper sample
    self.activeSamples.erase(self.activeSamples.begin() + activeSampleIdx);
  }
}
