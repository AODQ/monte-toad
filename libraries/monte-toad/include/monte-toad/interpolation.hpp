#pragma once

#include <array>
#include <tuple>

template <typename ElementType> struct span; // fwd decl

namespace mt::interpolation {

  // Takes a value x & performs spline-based 1D interpolation with the provided
  // nodes, computing the weights relative to nodes, & storing the weightsOffset
  // length into offsetOut. Returns true if any data was stored
  // adapted from PBRT-v3
  bool CatmullRomWeights(
    span<float const> nodes
  , float const x
  , size_t & offsetOut, std::array<float, 4> & weightsOut
  );

  std::tuple<float /*sample*/, float /*fnValue*/, float /*pdf*/>
  SampleCatmullRom2D(
    span<float const> nodes0, span<float const> nodes1
  , span<float const> values, span<float const> cdf
  , float const alpha, float uniform
  );

  // Implements fourier interpolation, adapted from PBRT-v3
  float Fourier(
    span<float const> alphaCoefficients
  , float const cosPhi
  );

  std::tuple<float /*sample*/, float /*pdf*/, float /*phi*/>
  SampleFourier(
    span<float const> alphaCoefficients
  , span<float const> reciprocals
  , float uniform
  );
}
