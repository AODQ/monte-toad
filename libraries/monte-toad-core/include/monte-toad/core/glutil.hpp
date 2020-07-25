#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

// simple RAII opengl utils, but this defaults to nothing if OpenGL was not
//   compiled in (this behaviour not yet implemented)

// OpenGL3.2 baseline so a lot of modern OpenGL (DSA, compute shaders,
//   BufferStorage etc) is sadly not used

namespace mt::core {
  struct GlTexture {
    GlTexture() = default;
    GlTexture(GlTexture const &) = delete;
    GlTexture(GlTexture &&);
    ~GlTexture();

    uint32_t handle;

    void Construct(uint32_t target);

    void Free();
  };

  struct GlBuffer {
    GlBuffer() = default;
    GlBuffer(GlBuffer const &) = delete;
    GlBuffer(GlBuffer &&);
    ~GlBuffer();

    uint32_t handle;

    void Construct(uint32_t target, size_t size, int32_t usageHints);

    void Free();
  };

  struct GlProgram {
    GlProgram() = default;
    GlProgram(GlProgram const &) = delete;
    GlProgram(GlProgram &&);
    ~GlProgram();

    uint32_t handle;

    void Construct(
      std::vector<std::tuple<std::string, uint32_t>> const & sources
    );

    void Free();
  };
}
