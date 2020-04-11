#pragma once

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

////////////////////////////////////////////////////////////////////////////////
template<> struct fmt::formatter<glm::vec2> {
  constexpr auto parse(format_parse_context & ctx) { return ctx.begin(); }

  template <typename FmtCtx> auto format(glm::vec2 const & vec, FmtCtx & ctx) {
    return format_to(ctx.out(), "({}, {})", vec.x, vec.y);
  }
};

////////////////////////////////////////////////////////////////////////////////
template<> struct fmt::formatter<glm::vec3> {
  constexpr auto parse(format_parse_context & ctx) { return ctx.begin(); }

  template <typename FmtCtx> auto format(glm::vec3 const & vec, FmtCtx & ctx) {
    return format_to(ctx.out(), "({}, {}, {})", vec.x, vec.y, vec.z);
  }
};


////////////////////////////////////////////////////////////////////////////////
template<> struct fmt::formatter<glm::vec4> {
  constexpr auto parse(format_parse_context & ctx) { return ctx.begin(); }

  template <typename FmtCtx> auto format(glm::vec3 const & vec, FmtCtx & ctx) {
    return format_to(ctx.out(), "({}, {}, {})", vec.x, vec.y, vec.z);
  }
};

