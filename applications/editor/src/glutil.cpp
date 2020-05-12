#include "glutil.hpp"

#include <monte-toad/log.hpp>

#include <glad/glad.hpp>

////////////////////////////////////////////////////////////////////////////////
GlBuffer::GlBuffer(GlBuffer && other) 
  : handle(other.handle)
{
  other.handle = 0;
}

////////////////////////////////////////////////////////////////////////////////
GlBuffer::~GlBuffer() {
  this->Free();
}

////////////////////////////////////////////////////////////////////////////////
void GlBuffer::Construct() {
  glCreateBuffers(1, &this->handle);
}

////////////////////////////////////////////////////////////////////////////////
void GlBuffer::Free() {
  if (this->handle != 0) { glDeleteBuffers(1, &this->handle); }
  this->handle = 0;
}

////////////////////////////////////////////////////////////////////////////////
GlTexture::GlTexture(GlTexture && other) 
  : handle(other.handle)
{
  other.handle = 0;
}

////////////////////////////////////////////////////////////////////////////////
GlTexture::~GlTexture() {
  this->Free();
}

////////////////////////////////////////////////////////////////////////////////
void GlTexture::Construct(uint32_t target) {
  glCreateTextures(target, 1, &this->handle);
}

////////////////////////////////////////////////////////////////////////////////
void GlTexture::Free() {
  if (this->handle != 0) { glDeleteTextures(1, &this->handle); }
  this->handle = 0;
}

////////////////////////////////////////////////////////////////////////////////
GlProgram::GlProgram(GlProgram && other) 
  : handle(other.handle)
{
  other.handle = 0;
}

////////////////////////////////////////////////////////////////////////////////
GlProgram::~GlProgram() {
  this->Free();
}

////////////////////////////////////////////////////////////////////////////////
void GlProgram::Construct(
  std::vector<std::tuple<std::string, uint32_t>> const & sources
) {
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

////////////////////////////////////////////////////////////////////////////////
void GlProgram::Free() {
  if (this->handle != 0) { glDeleteProgram(this->handle); }
  this->handle = 0;
}
