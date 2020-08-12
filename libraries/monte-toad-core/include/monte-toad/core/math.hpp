#pragma once

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
  #include <glm/fwd.hpp>
#pragma GCC diagnostic pop

namespace glm {
constexpr static float Pi     = 3.141592653589793f;
constexpr static float Pi2    = Pi*Pi;
constexpr static float InvPi  = 0.318309886183791f;
constexpr static float Tau    = 6.283185307179586f;
constexpr static float InvTau = 0.159154943091895f;
inline float sqr(float i) { return i*i; }
inline float rcp(float i) { return (1.0f/i); }
template <typename T> T saturate(T const & t) {
  return glm::clamp(t, T(0), T(1));
}

// returns true if any component of a is greater than b
bool GreaterThan(glm::vec3 a, glm::vec3 b);

// Calculates difference between azimuth angles; ф = фᵢ - фₒ
float CosDPhi(glm::vec3 const & wi, glm::vec3 const & wo);

inline float Sin2Theta(glm::vec3 const & w) {
  return glm::max(0.0f, 1.0f - (w.z*w.z));
}
}

#define SQR(X) ((X)*(X))
