#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <string>

////////////////////////////////////////////////////////////////////////////////
struct Buffer {
  Buffer() = default;
private:
  std::vector<glm::vec3> data = {};
  size_t width = 0, height = 0;

public:
  static Buffer Construct(size_t width, size_t height);

  float AspectRatio() const
    { return this->height/static_cast<float>(this->width); }

  size_t Width() const { return this->width; }
  size_t Height() const { return this->height; }
  glm::vec2 Dim() const { return glm::vec2(this->width, this->height); }

  glm::vec3 & At(size_t x, size_t y) { return this->data[y*this->width + x]; }

  glm::vec3 const & At(size_t x, size_t y) const
    { return this->data[y*this->width + x]; }

  glm::vec3 & At(size_t it) { return this->At(it%this->width, it/this->width); }
  glm::vec3 const & At(size_t it) const
    { return this->At(it%this->width, it/this->width); }
};

////////////////////////////////////////////////////////////////////////////////
void PrintProgress(float progress);

////////////////////////////////////////////////////////////////////////////////
void SaveImage(
  Buffer const & data
, std::string const & filename
, bool displayProgress
);
