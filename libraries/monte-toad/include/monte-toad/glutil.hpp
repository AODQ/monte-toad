#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

// simple RAII opengl utils

namespace mt {
  struct GlBuffer {
    GlBuffer() = default;
    GlBuffer(GlBuffer const &) = delete;
    GlBuffer(GlBuffer &&);
    ~GlBuffer();

    uint32_t handle;

    void Construct();

    void Free();
  };

  struct GlTexture {
    GlTexture() = default;
    GlTexture(GlTexture const &) = delete;
    GlTexture(GlTexture &&);
    ~GlTexture();

    uint32_t handle;

    void Construct(uint32_t target);

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
