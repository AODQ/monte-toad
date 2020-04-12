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
Noise<NoiseType::WhiteCached> Noise<NoiseType::WhiteCached>::Construct(
  size_t samples
) {
  Noise<NoiseType::WhiteCached> self;

  self.rng = std::mt19937(std::random_device()());
  { // -- generate whitenoise samples
    auto dist = std::uniform_real_distribution<float>(0.0f, 1.0f);
    self.samples.reserve(samples);
    for (size_t i = 0; i < samples; ++ i) {
      self.samples.emplace_back(glm::vec2(dist(self.rng), dist(self.rng)));
    }
  }
  self.dist =
    std::uniform_int_distribution<uint32_t>(0u, self.samples.size()-1);

  return self;
}

////////////////////////////////////////////////////////////////////////////////
float SampleUniform1(Noise<NoiseType::WhiteCached> & self) {
  return SampleUniform2(self).x;
}

////////////////////////////////////////////////////////////////////////////////
glm::vec2 SampleUniform2(Noise<NoiseType::WhiteCached> & self) {
  return self.samples[self.dist(self.rng)];
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
  auto gp = glm::uvec2(p.x/self.cellSize, p.y/self.cellSize);
  // TODO I believe below is a bug and should never be checked
  /* if (gp.x >= self.indices.size()) gp.x = self.dimensions - 1; */
  /* if (gp.y >= self.indices.size()) gp.y = self.dimensions - 1; */
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
  /* int32_t const adjacentDist = self.dimensions / 10; */
  /* int32_t const adjacentDist = self.dimensions; */
  int32_t const adjacentDist = 5;

  for (auto i = gp.x - adjacentDist; i < gp.x + adjacentDist; ++ i)
  for (auto j = gp.y - adjacentDist; j < gp.y + adjacentDist; ++ j) {
    // apply wrap-around to make the poisson distribution toroidal
    auto const idx = self.indices[i % self.dimensions][j % self.dimensions];
    if (idx == static_cast<size_t>(-1)) { continue; }

    glm::vec2 const testPoint = self.points[idx];

    if (ToroidalDistance(testPoint, point) < self.minDistance)
      { return true; }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Fast Poisson Disk Sampling in Arbitrary Dimensions
// cct.lsu.edu/~fharhad/ganbatte/siggraph2007/CD2/content/sketches/0250.pdf
std::vector<glm::vec2> PoissonDiskSample(
  uint32_t const numPoints
, uint32_t const sampleLimit = 32
, float minDistance = -1.0f
) {
  auto rng = std::mt19937(std::random_device()());
  auto distUniform = std::uniform_real_distribution<float>(0.0f, 1.0f);

  if (minDistance < 0.0f)
    { minDistance = glm::sqrt(numPoints)/static_cast<float>(numPoints); }

  // The final list of all samples
  std::vector<glm::vec2> samples;

  // all active samples; any new sample is added to this list, and every
  // iteration, `sampleLimit` number of points are generated based off one of
  // these active samples, the active sample is then which removed.
  std::vector<glm::vec2> activeSamples;

  // grid allows speedup of sample-neighborhood checks, as otherwise would have
  // to check every single sample
  auto grid = PoissonGrid::Construct(minDistance);

  // -- generate first point to base other samples from
  glm::vec2 firstPoint = glm::vec2(distUniform(rng), distUniform(rng));

  activeSamples.emplace_back(firstPoint);
  samples.emplace_back(glm::vec2(firstPoint.x, firstPoint.y));
  Insert(grid, firstPoint);

  // -- while there are active samples left to sample from & target sample goal
  //    has not been met
  while (!activeSamples.empty() && samples.size() < numPoints) {
    glm::vec2 activePoint;
    size_t activeSampleIdx;
    { // pop a random point
      std::uniform_int_distribution<size_t> dist(0u, activeSamples.size()-1);
      activeSampleIdx = dist(rng);
      activePoint = activeSamples[activeSampleIdx];
    }

    // -- generate `sampleLimit` number of points around the chosen activePoint
    bool found = false;
    for (size_t i = 0; i < sampleLimit; ++ i) {
      glm::vec2 randomPoint;
      { // generate random point near active point
        float const
          radius = minDistance * (distUniform(rng) + 1.0f)
        , theta  = Tau * distUniform(rng);

        randomPoint =
          glm::vec2(
            activePoint.x + radius*glm::cos(theta)
          , activePoint.y + radius*glm::sin(theta)
          );

        // since this point could lie outside of range [0, 1], apply wrapping
        randomPoint = glm::mod(randomPoint, glm::vec2(1.0f));
      }

      // -- if point is in bounds & in range of grid, it is valid; add to list
      if (InRange(grid, randomPoint)) { continue; }

      found = true;
      activeSamples.emplace_back(randomPoint);
      samples.emplace_back(glm::vec2(randomPoint.x, randomPoint.y));
      Insert(grid, randomPoint);
      break;
    }

    if (!found) {
      activeSamples.erase(activeSamples.begin() + activeSampleIdx);
    }
  }

  return samples;
}

////////////////////////////////////////////////////////////////////////////////
Noise<NoiseType::Blue> Noise<NoiseType::Blue>::Construct(size_t samples) {
  Noise<NoiseType::Blue> self;

  self.samples = PoissonDiskSample(samples, 32, 1.0f);
  self.it = 0;

  return self;
}

////////////////////////////////////////////////////////////////////////////////
float SampleUniform1(Noise<NoiseType::Blue> & self) {
  return SampleUniform2(self).x;
}

////////////////////////////////////////////////////////////////////////////////
glm::vec2 SampleUniform2(Noise<NoiseType::Blue> & self) {
  auto s = self.samples[self.it];
  if (++self.it >= self.samples.size()) {
    self.it = 0;
    std::shuffle(
      self.samples.begin(), self.samples.end()
    , std::mt19937(std::random_device()())
    );
  }
  return s;
}

////////////////////////////////////////////////////////////////////////////////
float     SampleUniform1(GenericNoiseGenerator & noise) {
  return
    std::visit([](auto && arg) -> float { return SampleUniform1(arg); }, noise);
}

glm::vec2 SampleUniform2(GenericNoiseGenerator & noise) {
  return
    std::visit(
      [](auto && arg) -> glm::vec2 { return SampleUniform2(arg); }
    , noise
    );
}
