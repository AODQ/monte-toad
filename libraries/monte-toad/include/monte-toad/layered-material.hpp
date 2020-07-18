#pragma once

#include <monte-toad/enum.hpp>
#include <monte-toad/math.hpp>

#include <vector>
#include <cstdint>
#include <tuple>

// -- fwd decl
namespace mt { struct PluginInfoRandom; struct SurfaceInfo; }

// -- decl

namespace mt::material {

  enum LayerType {
    Medium, Surface
  };

  enum SpectralChannel {
    Monochromatic = 1,
    Rgb = 3
  };

  struct Material {
    LayerType layerType;
  };

  struct FourierBsdfTable {
    float eta; // relative index of reffraction ; eta(bottom)/eta(top)
    size_t maxCoefficientOrders;

    // note that in the case of RGB, the spectral channel is actually stored as
    // Red-Blue-Luminance
    SpectralChannel spectralChannel;

    // represent number of zenith directions that have been discretized
    size_t muAngleDirections;

    // Sorted mu angles (μₛ) to allow binary search
    std::vector<float> sortedMuAngles;

    // order is always bounded by maxCoefficientOrders but varies
    //   with respect to μᵢ and μ; query this matrix to grab necessary order
    // To query, perform two binary searches in the discretized directions μ
    //   given by sortedMuAngles μₛ;
    //     μₛ[i] <= μᵢ < μ[i+1]
    //     μₛ[o] <= μₒ < μ[o+1]
    // Thus the required order can be fetched from the below array as
    //     [o*muAngleDirections + i]
    std::vector<size_t> coefficientOrderQuery;

    // Stores all αₖ coefficients for all pairs of discretized directions μ.
    // Because the maximum order varies can can even be zero, dinding the
    // offset into this array is a two-step process;
    //
    //  1.
    //    Offsets i and o are used to index into alphaCoefficientOffsets array
    //    to get an offset into alphaCoefficients;
    //      offset = alphaCoefficientOffsets[o*muAngleDirections + i]
    //    Thus the alphaCoefficientOffsets array has muAngleDirections² entries
    //
    //  2.
    //    The coefficientOrderQuery starting at alphaCoefficient[offset] give
    //      the αₖ values for the corresponding pair of directions. The first
    //      coefficients after alphaCoefficient[offset] encode coefficients for
    //      luminance, followed by red and then blue channel
    std::vector<size_t> alphaCoefficientOffsets;
    std::vector<float> alphaCoefficients;

    std::vector<float> alphaCoefficients0;
    std::vector<float> cdf;

    // contains precomputed integer reciprocals 1/i for all mMax fourier orders
    std::vector<float> reciprocals;
  };

  FourierBsdfTable Load();

  struct LayeredMaterial {
    std::vector<Material> layers;
  };

  FourierBsdfTable SolveScatteringMatrices(
    LayeredMaterial materialStructure
  , size_t const n, size_t const fourierModes
  );

  glm::vec3 BsdfFs(
    mt::material::FourierBsdfTable const & fourierBsdf
  , mt::SurfaceInfo const & surface
  , glm::vec3 const & outgoingAngle
  , mt::TransportMode transportMode
  );

  float BsdfPdf(
    mt::material::FourierBsdfTable const & fourierBsdf
  , mt::SurfaceInfo const & surface
  , glm::vec3 const & outgoingAngle
  );

  std::tuple<glm::vec3 /*outgoingAngle*/, glm::vec3 /*bsdffs*/, float /*pdf*/>
  BsdfSample(
    mt::material::FourierBsdfTable const & fourierBsdf
  , mt::PluginInfoRandom const & random
  , mt::SurfaceInfo const & surface
  , mt::TransportMode transportMode
  );
}
