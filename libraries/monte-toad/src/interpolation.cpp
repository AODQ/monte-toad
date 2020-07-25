#include <monte-toad/interpolation.hpp>

#include <monte-toad/core/math.hpp>
#include <monte-toad/core/span.hpp>

#include <functional>

namespace {

// Performs binary search over the interval using provided function as predicate
// code adopted from PBRT-v3
template <typename Fn>
size_t FindInterval(size_t const size, Fn const & predicate) {
  size_t first = 0lu, len = size;
  while (len > 0lu) {
    size_t half = len >> 1, middle = first + half;
    // bisect range based on value of pred at middle
    if (predicate(middle)) {
      first = middle + 1;
      len -= half + 1;
    } else {
      len = half;
    }
  }

  return glm::clamp(first - 1, 0lu, size - 2);
}

} // -- namespace

// -- adapted from PBRT-v3
/*
  catmull-rom spline is a weighted sum over four control points, where weight
    and control points depend on parametric location along the curve's path for
    the value 'x' being computed.

  Consider a set of values from a function f and derivatives f' at positions
    x₀ .. xₖ, approximating the function for an interval [xᵢ, xₛ] using a cubic
    polynomial;

      f(x) = a*x³ + b*x² + c*x + d

        a = f'(xᵢ) + f'(xₛ) + 2*f(xᵢ) - 2*f(xₛ)
        b = 3*f(xₛ) - 3*f(xᵢ) - 2*f'(xᵢ) - f'(xₛ)
        c = f'(xᵢ)
        d = f(xᵢ)

        thus

      f(x) =
          f(xᵢ)  * ( 2*x³ - 3*x² + 1)
        + f(xₛ)  * (-2*x³ + 3*x²    )
        + f'(xᵢ) * (   x³ - 2*x² + x)
        + f'(xₛ) * (   x³ - x²      )

    This is too restrictive since a derivative of the function would need to be
      analytically provided. Instead this can be estimated using central
      differences based on adjacent function values;

                   f(x₂) - f(x₀)
        f'(x₁) ~= ----------------
                        x₂

    more information of this can be found in 8.26 of PBRT-v3

    the weights, now independent of function values, are now used in
      interpolation as;

         f(x) = w₀*f(x₋₁) + w₁*f(x₀) + w₂*f(x₁) + w₃*f(x₂)

          x³ - 2*x² + x
    w₀ = ---------------
             x₋₁ - 1

    w₁ =  2*x³ - 3*x² + 1 - w₃
    w₂ = -2*x³ + 3*x²     - w₀

          x³ - x²
    w₃ = ---------
            x²
*/
bool mt::interpolation::CatmullRomWeights(
  span<float const> nodes
, float x
, size_t & offsetOut, std::array<float, 4> & weightsOut
) {
  // check if x is out of bounds, use negated form to catch NaN
  if (!(x >= nodes.front() && x <= nodes.back())) { return false; }

  // search for interval idx containing x
  size_t const idx =
    FindInterval(nodes.size(), [&](size_t i) { return nodes[i] <= x; });

  offsetOut = idx - 1;
  float const x0 = nodes[idx], x1 = nodes[idx+1];

  // compute t param & powers
  float const t = (x-x0)/(x1 - x0), t2 = t*t, t3 = t2*t;

  // compute weights
  weightsOut[1] =  2.0f*t3 - 3.0f*t2 + 1.0f;
  weightsOut[2] = -2.0f*t3 + 3.0f*t2;

  // compute first weight, cornercase where idx is 0
  if (idx > 0) {
    float const w0 = (t3 - 2.0f*t2 + t)  * (x1-x0)/(x1 - nodes[idx - 1]);
    weightsOut[0] = -w0;
    weightsOut[2] += w0;
  } else {
    float const w0 = t3 - 2.0f*t2 + t;
    weightsOut[0] = 0;
    weightsOut[1] -= w0;
    weightsOut[2] += w0;
  }

  // compute last weight, cornercase where idx is last element
  if (idx+2 < nodes.size()) {
    float const w3 = (t3 - t2) * (x1 - x0)/(nodes[idx+2] - x0);
    weightsOut[1] -= w3;
    weightsOut[3] = w3;
  } else {
    float const w3 = t3 - t2;
    weightsOut[1] -= w3;
    weightsOut[2] += w3;
    weightsOut[3] = 0.0f;
  }

  return true;
}

// adapted from PBRT-3v
std::tuple<float /*sample*/, float /*fnValue*/, float /*pdf*/>
mt::interpolation::SampleCatmullRom2D(
  span<float const> nodes0, span<float const> nodes1
, span<float const> values, span<float const> cdf
, float const alpha, float uniform
) {
  // determine offset and coefficients for α
  size_t offset;
  std::array<float, 4> weights;
  if (!mt::interpolation::CatmullRomWeights(nodes0, alpha, offset, weights))
    { return { 0.0f, 0.0f, 0.0f }; }

  // define λ function to interpolate entries
  auto interpolate = [&](span<float const> array, size_t idx) {
    float value = 0.0f;
    for (size_t i = 0; i < 4; ++ i) {
      if (weights[i] == 0.0f) { continue; }
      value += array[(offset + i) * nodes1.size() + idx] * weights[i];
    }
    return value;
  };

  // map uniform to a spline interval by inverting the interpolated CDF
  float const maximum = interpolate(cdf, nodes1.size()-1);
  uniform *= maximum;

  size_t const idx =
    FindInterval(
      nodes1.size()
    , [&](size_t i) { return interpolate(cdf, i) <= uniform; }
    );

  // look up node positions and interpolated function values
  float const f0 = interpolate(values, idx), f1 = interpolate(values, idx+1);
  float const x0 = nodes1[idx], x1 = nodes1[idx+1];
  float const width = x1 - x0;
  float d0, d1;

  // rescale uniform using interpolated cdf
  uniform = (uniform - interpolate(cdf, idx)) / width;

  // approximate derivates using finite differences of the interpolant
  d0 =
      idx > 0
    ? width * (f1 - interpolate(values, idx - 1)) / (x1 - nodes1[idx - 1])
    : f1 - f0;

  d1 =
      idx+2 < nodes1.size()
    ? width * (interpolate(values, idx+2) - f0) / (nodes1[idx+2] - x0)
    : f1 - f0;

  // -- invert definite integral over spline segment and return solution
  // inverted integral must be solved;
  //
  //    Fᵢ⁻¹ (ξ₁ F(n-1) - Fᵢ
  //
  //  Fᵢ is the integral over cube spline segment fᵢ, which makes it a quartic
  //    polynomial. This can be solved numerically bc of the following;
  //
  //  1. The function Fᵢ increases monotonically
  //  2. interval [xᵢ, xᵢ₊₁] selected by `FindInterval` has only one solution
  //
  //  because of the bracketing interval, a bisection search can be used. It is
  //    an iterative root-finding technique gaurunteed to converge to the
  //    solution. The bisection search splits interval into two parts and
  //    discards the subinterval that does not bracket the solution. The method
  //    is linear, but PBRT-v3 uses Newton-Bisection, which is a combination of
  //    quadratically converging & unsafe Newton's method using bisection
  //    search as a fallback

  // set initial guess for t by importance sampling a linear interpolation
  float t =
      f0 != f1
    ? (f0 - glm::sqrt(glm::max(0.0f, f0*f0 + 2.0f*uniform*(f1-f0)))) / (f0-f1)
    : uniform / f0;

  float a = 0.0f, b = 1.0f, Fhat, fhat;
  while (true) {
    // fall back to a bisection step when t is out of bounds
    if (!(t >= a && t <= b)) { t = 0.5f * (a + b); }

    // evaluate target function and its derivative in Horner form
    Fhat =
      t
    * ( f0 + t
      * ( 0.5f * d0 + t
        * ( (1.f/3.f) * (-2 * d0 - d1)
          + f1 - f0 + t * (.25f * (d0 + d1) + .5f * (f0 - f1))
          )
        )
      );

    fhat =
      f0
    + t
    * ( d0 + t * (-2 * d0 - d1 + 3 * (f1 - f0)
      + t * (d0 + d1 + 2 * (f0 - f1)))
      );

    // stop iteration if converged
    if (glm::abs(Fhat - uniform) < 1e-6f || b - a < 1e-6f) { break; }

    // update bisection bounds using updated t
    if (Fhat - uniform < 0.0f)
      { a = t; }
    else
      { b = t; }

    // perform a newton step
    t -= (Fhat - uniform) / fhat;
  }

  // return the sample position and function value
  return { x0 + width*t, fhat, fhat/maximum };
}

float mt::interpolation::Fourier(
  span<float const> alphaCoefficients
, float const cosPhi
) {
  // use double in cosine iterators & sum to minimize precision errors
  double value = 0.0f;
  double cosKMinusOnePhi = static_cast<double>(cosPhi), cosKPhi = 1.0;

  for (auto & alphaCoefficient : alphaCoefficients) {
    // add currant sum & update cosine iterators
    value += alphaCoefficient * cosKPhi;
    float cosKPlusOnePhi = 2.0f * cosPhi*cosKPhi - cosKMinusOnePhi;
    cosKMinusOnePhi = cosKPhi;
    cosKPhi = cosKPlusOnePhi;
  }

  return value;
}

// a lot of theory / code here borrows from the same procedurs in
//   SampleCatmullRom2D; there is still trying to binary-search the solution
//   using newton's method, with bisection steps as a backup
std::tuple<float /*sample*/, float /*pdf*/, float /*phi*/>
mt::interpolation::SampleFourier(
  span<float const> alphaCoefficients
, span<float const> reciprocals
, float uniform
) {
  // pick side and declare bisection variables
  bool const flip = (uniform >= 0.5f);
  uniform = flip ? 1.0f - 2.0f*(uniform - 0.5f) : uniform * 2.0f;

  // iterators, use double to avoid floating point precision
  double a = 0.0f, b = glm::Pi, phi = 0.5f * glm::Pi;

  // -- evaluate F(ф) and derivative f(ф)
  float F, f;
  while (true) {
    // initialize sine and cosine iterators, use double to avoid floating point
    //   precision
    double
      cosPhi = glm::cos(static_cast<double>(phi))
    , sinPhi = glm::sqrt(1.0 - cosPhi*cosPhi)
    , cosPhiPrev =  cosPhi, cosPhiCur = 1.0
    , sinPhiPrev = -sinPhi, sinPhiCur = 0.0
    ;

    // initialize F and f with first series term
    F = alphaCoefficients[0] * phi;
    f = alphaCoefficients[0];

    for (size_t coeffIt = 1; coeffIt < alphaCoefficients.size(); ++ coeffIt) {
      // compute next sine and cosine iterators
      double
        sinPhiNext = 2.0 * cosPhi * sinPhiCur - sinPhiPrev
      , cosPhiNext = 2.0 * cosPhi * cosPhiCur - cosPhiPrev;
      sinPhiPrev = sinPhiCur; sinPhiCur = sinPhiNext;
      cosPhiPrev = cosPhiCur; cosPhiCur = cosPhiNext;

      // add next series term to F and f
      F += alphaCoefficients[coeffIt] * reciprocals[coeffIt] * sinPhiNext;
      f += alphaCoefficients[coeffIt] * cosPhiNext;
    }
    F -= uniform * alphaCoefficients[0] * glm::Pi;

    // update bisection bounds using updated ф
    if (F > 0.0f)
      { b = phi; }
    else
      { a = phi; }

    // stop fourier bisection iteration if converged
    if (glm::abs(F) < 1e-6 || b - a < 1e-6f) { break; }

    // perform Newton step given f(ф) and F(ф)
    phi -= F / f;

    // fall back to bisection step when ф OOB
    if (!(phi >= a && phi <= b)) { phi = 0.5f * (a + b); }
  }

  // flip ф if necessary
  if (flip) { phi = 2.0f * glm::Pi - phi; }

  return { f, f/(2.0f * glm::Pi * alphaCoefficients[0]), phi };
}
