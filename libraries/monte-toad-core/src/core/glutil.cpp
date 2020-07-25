#include <monte-toad/core/glutil.hpp>

#include <monte-toad/core/log.hpp>

#include <glad/glad.hpp>

mt::core::GlTexture::GlTexture(GlTexture && other) 
  : handle(other.handle)
{
  other.handle = 0;
}

mt::core::GlTexture::~GlTexture() {
  this->Free();
}

void mt::core::GlTexture::Construct(uint32_t target) {
  if (this->handle != 0) { this->Free(); }
  glGenTextures(1, &this->handle);
  glBindTexture(target, this->handle);
}

void mt::core::GlTexture::Free() {
  if (this->handle != 0) { glDeleteTextures(1, &this->handle); }
  this->handle = 0;
}

// -- gl buffer

mt::core::GlBuffer::GlBuffer(GlBuffer && other) 
  : handle(other.handle)
{
  other.handle = 0;
}

mt::core::GlBuffer::~GlBuffer() {
  this->Free();
}

void mt::core::GlBuffer::Construct(
  uint32_t target
, size_t size
, int32_t usageHints
) {
  if (this->handle != 0) { this->Free(); }
  glGenBuffers(1, &this->handle);
  glBindBuffer(target, this->handle);
  glBufferData(target, size, nullptr, usageHints);
}

void mt::core::GlBuffer::Free() {
  if (this->handle != 0) { glDeleteBuffers(1, &this->handle); }
  this->handle = 0;
}

mt::core::GlProgram::GlProgram(GlProgram && other) 
  : handle(other.handle)
{
  other.handle = 0;
}

mt::core::GlProgram::~GlProgram() {
  this->Free();
}

void mt::core::GlProgram::Construct(
  std::vector<std::tuple<std::string, uint32_t>> const & sources
) {
  if (this->handle != 0) { this->Free(); }
  this->handle = glCreateProgram();
  { // compile program
    std::vector<uint32_t> shaders;
    shaders.reserve(sources.size());

    for (auto const & source : sources) {
      shaders.emplace_back(glCreateShader(std::get<1>(source)));
      auto & shader = shaders.back();
      char const * shaderData = std::get<0>(source).c_str();
      glShaderSource(shader , 1, &shaderData , nullptr);
      glCompileShader(shader);
      glAttachShader(this->handle, shader);
    }

    glLinkProgram(this->handle);
    for (auto const & shader : shaders) { glDeleteShader(shader); }
  }

  { // check for errors
    int32_t success;
    glGetProgramiv(this->handle, GL_LINK_STATUS, &success);

    if (!success) {
      std::array<char, 4096> buffer;
      glGetProgramInfoLog(this->handle, 4096, nullptr, buffer.data());
      spdlog::error("Shader error: {}\n", buffer.data());
    }
  }
}

void mt::core::GlProgram::Free() {
  if (this->handle != 0) { glDeleteProgram(this->handle); }
  this->handle = 0;
}
