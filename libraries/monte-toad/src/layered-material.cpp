//#include <monte-toad/layered-material.hpp>
//
//#include <monte-toad/interpolation.hpp>
//#include <monte-toad/log.hpp>
//#include <monte-toad/span.hpp>
//#include <monte-toad/surfaceinfo.hpp>
//#include <mt-plugin/plugin.hpp>
//
//#include <array>
//
///*
//  This is an implementation of
//    "A Comprehensive Framework for Rendering Layered Materials"
//    Wenzel Jakob , Eugen D'Eon, Otto Jakob, Steve Marschner
//
//  A lot of comment/code here is based directly off that paper or the PBR-3rd ed
//    chapter on the same topic
//
//  ---
//
//  The premise is that BSDFs can support multiple-layered materials;
//
//
//  Solving this is computationally expensive because of the scattering in
//    between each layer. Each surface interaction would essentially result in
//    having to solve a one-dimensional light transport equation.
//
//  One observation is that the BSDF can be calculated offline in a large table,
//    first the BSDF function is decomposed to represent isotropic spherical
//    coordinates;
//
//      f(ωᵢ, ωₒ) = f(μᵢ, фᵢ, μₒ, фₒ)
//        where μ is cosine and ф is azimuth angles
//
//  Thus a 4D lookup table of these values can be generated; the problem is the
//    amount of storage required for this is highly impractible.
//
//  The BSDF can instead be represented using Fourier series, where a sum of
//    scaled cosine terms can efficiently, sparsely, and accurately store
//    frequencies that represent the lookup table. It has an added benefit that
//    the fourier table can be importance sampled.
//
//  The major cost of this is that each BRDF supported by this system needs to be
//    transformed into a fourier sum, either analytically or numerically. The
//    fourier series also does not work well with perfectly-specular surfaces,
//    but this is not a major problem.
//
//  *----------------------------------------------------------------------*
//  | symbol        | definition                                           |
//  *---------------+------------------------------------------------------*
//  | Φ(τ, μ, ф)    | continous radiance function                          |
//  | μ             | cosine elevation angle                               |
//  | ф             | azimuth angle                                        |
//  | τ             | optical depth within a layer                         |
//  | n             | number of discretizations in μ                       |
//  | m             | number of fourier basis functions                    |
//  | δ(i, j)       | kronecker delta                                      |
//  | l             | index used for fourier expansions                    |
//  | α             | beckmann roughness of a layer                        |
//  | σ             | extinction coefficient of a layer                    |
//  | p             | phase function of a layer                            |
//  | f             | BSDF of a boundary between layers                    |
//  | F             | fourier coefficient                                  |
//  | Φₗ(μ), Φₗ     | fourier expansion of ф and μ-discretization R^N      |
//  | pₗ(μ, μ'), Pₗ | fourier expansion of p and μ-discretization R^(NxN)  |
//  | fₗ(μ, μ'), Fₗ | fourier expansion of f and μ-discretization R^(NxN)  |
//  | W             | integration weights of the quadrature scheme R^(NxN) |
//  *----------------------------------------------------------------------*
//
//  The fourier BSDFs (for now) only support isotropic BSDFs. This means that the
//    BSDF polar-coordinate function can be rewritten as;
//
//      f(μᵢ, фᵢ, μₒ, фₒ) = f(μᵢ, μₒ, ф)
//        where ф = фᵢ - фₒ
//
//  The BSDF can be represented as Fourier series over azimuth angles;
//
//                       m-1
//    f(μᵢ, μₒ, ф) |μᵢ| = Σ  αₖ(μᵢ, μₒ) cos(k(ф))
//                        k
//
//      αₖ(μᵢ, μₒ)
//        represent Fourier coefficients for a pair of zenith angle cosines. The
//        functions αₖ are discretized over their input arguments.
//
//  With a set of zenith angle cosines μₛ = { μ₀, ‥ , αₘ₋₁(μᵢ, μₒ) }, αₖ values
//    are stored for every pair 0 <= i, j < n. Each αₖ can be thought of as an
//    nxn matrix, and the BSDF representation consists of a set of m such
//    matrices.  Each matrix descries a different azimuthal oscillation frequency
//    in the material's response to an incoming illumination.
//
//  The maximum order m needed to evaluate the fourier expansion depends on the
//    complexity of the BSDF for a given pair of directions. Choosing the correct
//    value is important to get good compactness of the BSDF.
//
//  Consider a nearly perfect specular surface; many coefficients are necessary
//    to accurately represent the specular lobe, which is zero for nearly all
//    azimuth angle differences ф = фᵢ - фₒ, yet large for a small set of
//    directions around ф = π, where incoming and outgoing directions are nearly
//    opposite. However, when μᵢ and μₒ aren't aligned, only a single term is
//    needed to represent that the BSDF is zero.
//
//  For smoother BSDFs, most or all pairs of μᵢ and μₒ require multiple
//    coefficients αₖ to represent their ф distribution, but not many
//    coefficients are needed for each αₖ. This means that these values can be
//   stored sparsely to reduce storage requirements drastically.
//
//*/
//
//
////------------------------------------------------------------------------------
////-- fourier table BSDF private functions --------------------------------------
////------------------------------------------------------------------------------
//namespace {
//
//// Gets αₖ coefficient for a pair of discretized directions offsetI/O,
//// returning the coefficient index and pointer to the alpha coefficient
//std::tuple<size_t, float const *> GetAlphaKCoefficient(
//  mt::material::FourierBsdfTable const & self
//, size_t const offsetI
//, size_t const offsetO
//) {
//  const auto offset = offsetO*self.muAngleDirections + offsetI;
//
//  return {
//    self.coefficientOrderQuery[offset]
//  , self.alphaCoefficients.data() + self.alphaCoefficientOffsets[offset]
//  };
//}
//
//// stores information necessary for coefficient catmull-rom interpolation of
////   fourier coefficients
//struct FourierCoefficientInterpolation {
//  size_t offsetI, offsetO;
//  std::array<float, 4> weightsI, weightsO;
//};
//
//bool ComputeOffsetsAndWeights(
//  FourierCoefficientInterpolation & interpOut
//, span<float const> muAngles, float const muI, float const muO
//) {
//  /*
//    offset into the αₖ array needs to be generated, and then extended out 4x4
//      with catmull-rom weights weights controlling each coefficient's
//      contribution.
//  */
//  return
//      !mt::interpolation::CatmullRomWeights(
//        muAngles, muI, interpOut.offsetI, interpOut.weightsI
//      )
//   || 
//     !mt::interpolation::CatmullRomWeights(
//       muAngles, muO, interpOut.offsetO, interpOut.weightsO
//     )
//  ;
//}
//
//} // -- namespace
//
////------------------------------------------------------------------------------
////-- fourier table BSDF public functions ---------------------------------------
////------------------------------------------------------------------------------
//
//glm::vec3 mt::material::BsdfFs(
//  mt::material::FourierBsdfTable const & fourierBsdf
//, mt::SurfaceInfo const & surface
//, glm::vec3 const & outgoingAngle
//, mt::TransportMode transportMode
//) {
//  /*
//    compute fourier coefficients αₖ for (μᵢ, μₒ), then applies Fourier
//      interpolation to gather the correct spectrum for BsdfFs
//  */
//
//  // find zenith angle cosines and azimuth difference angle
//  float const muI = surface.incomingAngle.z, muO = outgoingAngle.z;
//  float const cosPhi = glm::CosDPhi(surface.incomingAngle, outgoingAngle);
//
//  // apply fourier interpolation
//  FourierCoefficientInterpolation interp;
//  if (
//    !ComputeOffsetsAndWeights(
//      interp, make_span(fourierBsdf.sortedMuAngles), muI, muO
//    )
//  ) {
//    return glm::vec3(0.0f);
//  }
//
//  // -- temporary stack storage for weighted alpha coefficients
//  /*
//    αₖ vectors in a 4x4 extent should be interpolated over, but they may have
//      different coefficient orders; the maximum is given by
//      maxCoefficientOrders, but this has to be scaled by the
//      channels-per-spectrum. 3 for now but in spectral rendering this would be 1
//
//    for Rgb channels, they are in linear LLLRRRBBB (lum, red, blue) format
//  */
//  float * weightedAlphaK =
//    reinterpret_cast<float*>(
//      ::alloca(fourierBsdf.maxCoefficientOrders * 3 * sizeof(float))
//    );
//  ::memset(
//    weightedAlphaK
//  , 0 // clear to 0 to set to 0.0f
//  , fourierBsdf.maxCoefficientOrders * 3 * sizeof(float)
//  );
//
//  // calculate max coefficients found locally with the next step
//  size_t maxCoefficients = 0;
//
//  // -- accumulate weighted sums of neighbour αₖ coefficients
//  /*
//    Reconstructs the BSDF by interpolating multiple coefficients that were
//      gathered by catmull rom weights. The interpolation uses a tensor-product
//      spline, where weights for the sample function values are computed
//      seperately for each parameter and then multiplied together. Thus to
//      compute a final fourier coefficient;
//
//                3   3
//          αₖ =  Σ   Σ αₖ(oᵢ + itI, oₒ + itO) wᵢ(itI) wₒ(itO)
//               itO itB
//  */
//
//  for (size_t itO = 0; itO < 4; ++ itO)
//  for (size_t itI = 0; itI < 4; ++ itI) {
//    // add contribution of α_(itI, itO) value
//    float weight = interp.weightsI[itI] * interp.weightsO[itO];
//    if (weight == 0.0f) { continue; }
//
//    auto [localMaxCoeff, localAlphaK] =
//      ::GetAlphaKCoefficient(
//        fourierBsdf, interp.offsetI+itI, interp.offsetO+itO
//      );
//    maxCoefficients = glm::max(maxCoefficients, localMaxCoeff);
//
//    // contribute to coefficients for each channel
//    for (size_t channel = 0; channel < 3; ++ channel)
//    for (size_t coeffIt = 0; coeffIt < localMaxCoeff; ++ coeffIt) {
//      weightedAlphaK[channel*fourierBsdf.maxCoefficientOrders + coeffIt]
//        += weight * localAlphaK[channel*localMaxCoeff + coeffIt];
//    }
//  }
//
//  // -- evaluate Fourier expansion for angle ф
//
//  // with final αₖ coefficients now weighted, luminance can be calculated using
//  //   Fourier interpolation. The Fourier computes BSDF value for specified
//  //   channel, which in this case is either RGB luminance or the spectral
//  //   frequency
//  // errors in fourier reconstruction can manifest in negative values, thus max
//  float const luminance =
//    glm::max(
//      0.0f
//    , mt::interpolation::Fourier(
//        span<float const>(weightedAlphaK, maxCoefficients)
//      , cosPhi
//      )
//    );
//
//  // calculate cosine-weighted scale, since αₖ in fourier series represents
//  //   cosine-weighted BSDF, and must be removed;
//  //
//  //     f(μᵢ, μₒ, ф)|μᵢ| = Σ αₖ(μᵢ, μₒ) cos(k(ф))
//  //
//  // must also account for adjoint light transport, which applies relative IOR
//  //   iff refraction is occuring
//  float cosScale = muI == 0.0f ? 0.0f : (1.0f/glm::abs(muI));
//  if (transportMode == mt::TransportMode::Radiance && muI*muO > 0.0f) {
//    float const eta = muI > 0.0f ? 1.0f/fourierBsdf.eta : fourierBsdf.eta;
//    cosScale *= eta*eta;
//  }
//
//  switch (fourierBsdf.spectralChannel) {
//    case mt::material::SpectralChannel::Monochromatic:
//      // SPECTRAL NOT IMPLEMENTED YET
//      return glm::vec3(0.0f);
//    case mt::material::SpectralChannel::Rgb:
//      auto const & fourierMaxCoefficients = fourierBsdf.maxCoefficientOrders;
//
//      // calculate RGB channels, green channel can be calculated from luminance
//      glm::vec3 rgb = glm::vec3(0.0f);
//
//      rgb.r =
//        mt::interpolation::Fourier(
//          span<float const>(
//            weightedAlphaK + 1*fourierBsdf.maxCoefficientOrders
//          , maxCoefficients
//          )
//        , cosPhi
//        );
//      rgb.b =
//        mt::interpolation::Fourier(
//          span<float const>(
//            weightedAlphaK + 2*fourierMaxCoefficients
//          , maxCoefficients
//          )
//        , cosPhi
//        );
//
//      // yλ = 0.212671*r + 0.715160*g + 0.072169*b, solved for g
//      rgb.g = 1.39829f*luminance - 0.100913f*rgb.b - 0.297375f*rgb.r;
//
//      // errors in fourier reconstruction can manifest in negative values
//      return glm::max(glm::vec3(0.0f), rgb*cosScale);
//  }
//
//  assert(0);
//  return glm::vec3(0.0f);
//}
//
//// Pdf method for fourier returns solid angle desnity for the sampling method.
//float BsdfPdf(
//  mt::material::FourierBsdfTable const & fourierBsdf
//, mt::SurfaceInfo const & surface
//, glm::vec3 const & outgoingAngle
//) {
//  // find zenith angle cosine and azimuth difference angle
//  float muI = surface.incomingAngle.z, muO = outgoingAngle.z;
//  float cosPhi = glm::CosDPhi(surface.incomingAngle, outgoingAngle);
//
//  // apply fourier interpolation
//  FourierCoefficientInterpolation interp;
//  if (
//    !ComputeOffsetsAndWeights(
//      interp, make_span(fourierBsdf.sortedMuAngles), muI, muO
//    )
//  ) {
//    return 0.0f;
//  }
//
//
//  // -- temporary stack storage for weighted alpha coefficients
//  float * weightedAlphaK =
//    reinterpret_cast<float*>(
//      ::alloca(fourierBsdf.maxCoefficientOrders * 3 * sizeof(float))
//    );
//  ::memset(
//    weightedAlphaK
//  , 0 // clear to 0 to set to 0.0f
//  , fourierBsdf.maxCoefficientOrders * 3 * sizeof(float)
//  );
//
//  // calculate max coefficients found locally with the next step
//  size_t maxCoefficients = 0;
//
//  // -- accumulate luminance fourier coefficients αₖ for μᵢ, μₒ
//  // similar to BsdfFs implementation, except we don't need to worry about
//  //   multiple channels now since we're only interested in luminance
//  for (size_t itO = 0; itO < 4; ++ itO)
//  for (size_t itI = 0; itI < 4; ++ itI) {
//    float weight = interp.weightsI[itI] * interp.weightsO[itO];
//    if (weight == 0.0f) { continue; }
//
//    auto [localMaxCoeff, localAlphaK] =
//      ::GetAlphaKCoefficient(
//        fourierBsdf, interp.offsetI+itI, interp.offsetO+itO
//      );
//
//    maxCoefficients = glm::max(maxCoefficients, localMaxCoeff);
//
//    for (size_t coeffIt = 0; coeffIt < localMaxCoeff; ++ coeffIt) {
//      weightedAlphaK[coeffIt] += weight * localAlphaK[coeffIt];
//    }
//  }
//
//  // evaluate probability of sampling ωᵢ;
//  //
//  //  suppose μₒ is part of the discretized set of zenith angle cosines μ₀ ‥ μₛ
//  //
//  //      2π  π
//  //   =  ∫   ∫  f(μᵢ, μₒ, ф) |μᵢ| dμᵢ dф
//  //
//  //          ₁  1  π
//  //   = 2*π  ∫ --- ∫  f(μᵢ, μₒ, ф) |μᵢ| dф μ
//  //         ⁻¹  π
//  //
//  //          ₁
//  //   = 2*π  ∫ α₀(μᵢ, μₒ) dμᵢ
//  //         ⁻¹
//  //
//  //   = 2*π*I₍ₛ,ₒ₎
//  //
//  // where
//  //             μᵢ
//  //    I₍ᵢ,ₒ₎ = ∫ α₀(μ', μ₀) dμ'
//  //            ⁻¹
//  //
//  // which is stored in the array of precomputed CDFs
//
//  float rho = 0.0f;
//  for (size_t o = 0; o < 4; ++ o) {
//    if (interp.weightsO[o] == 0.0f) { continue; }
//    rho +=
//        interp.weightsO[o]
//      * fourierBsdf.cdf[
//          (interp.offsetO + o) * fourierBsdf.maxCoefficientOrders
//        + fourierBsdf.maxCoefficientOrders - 1
//        ]
//      * glm::Tau
//    ;
//  }
//
//  float const luminance =
//    mt::interpolation::Fourier(
//      span<float const>(weightedAlphaK, maxCoefficients)
//    , cosPhi
//    );
//
//  return (rho > 0.0f && luminance > 0.0f) ? (luminance/rho) : 0.0f;
//}
//
//std::tuple<glm::vec3 /*outgoingAngle*/, glm::vec3 /*bsdf*/, float /*pdf*/>
//BsdfSample(
//  mt::material::FourierBsdfTable const & fourierBsdf
//, mt::PluginInfoRandom const & random
//, mt::SurfaceInfo const & surface
//, mt::TransportMode transportMode
//) {
//
//  glm::vec2 uniform = random.SampleUniform2();
//
//  // sample zenith angle component
//  float muO = surface.incomingAngle.z;
//  auto [muI, _, pdfMu] =
//    mt::interpolation::SampleCatmullRom2D(
//      span<float const>(
//        fourierBsdf.sortedMuAngles.data(), fourierBsdf.maxCoefficientOrders
//      )
//    , span<float const>(
//        fourierBsdf.sortedMuAngles.data(), fourierBsdf.maxCoefficientOrders
//      )
//    , make_span(fourierBsdf.alphaCoefficients0)
//    , make_span(fourierBsdf.cdf)
//    , muO, uniform.y
//    );
//
//  // -- compute fourier coefficients αₖ for (μᵢ, μₒ)
//
//  // calculate interpolation offsets/weights
//  FourierCoefficientInterpolation interp;
//  if (
//    !ComputeOffsetsAndWeights(
//      interp, make_span(fourierBsdf.sortedMuAngles), muI, muO
//    )
//  ) {
//    return {glm::vec3(0.0f), glm::vec3(0.0f), 0.0f};
//  }
//
//  // -- temporary stack storage for weighted alpha coefficients
//  float * weightedAlphaK =
//    reinterpret_cast<float*>(
//      ::alloca(fourierBsdf.maxCoefficientOrders * 3 * sizeof(float))
//    );
//  ::memset(
//    weightedAlphaK
//  , 0 // clear to 0 to set to 0.0f
//  , fourierBsdf.maxCoefficientOrders * 3 * sizeof(float)
//  );
//
//  // calculate max coefficients found locally with the next step
//  //  exactly how it's done in BsdfFs, so maybe this can be moved to a function
//  size_t maxCoefficients = 0;
//
//  for (size_t itO = 0; itO < 4; ++ itO)
//  for (size_t itI = 0; itI < 4; ++ itI) {
//    // add contribution of α_(itI, itO) value
//    float weight = interp.weightsI[itI] * interp.weightsO[itO];
//    if (weight == 0.0f) { continue; }
//
//    auto [localMaxCoeff, localAlphaK] =
//      ::GetAlphaKCoefficient(
//        fourierBsdf, interp.offsetI+itI, interp.offsetO+itO
//      );
//    maxCoefficients = glm::max(maxCoefficients, localMaxCoeff);
//
//    // contribute to coefficients for each channel
//    for (size_t channel = 0; channel < 3; ++ channel)
//    for (size_t coeffIt = 0; coeffIt < localMaxCoeff; ++ coeffIt) {
//      weightedAlphaK[channel*fourierBsdf.maxCoefficientOrders + coeffIt]
//        += weight * localAlphaK[channel*localMaxCoeff + coeffIt];
//    }
//  }
//
//  // importance sample the luminance fourier expansion
//  auto [luminance, pdfPhi, phi] =
//    mt::interpolation::SampleFourier(
//      span<float const>(
//        fourierBsdf.alphaCoefficients.data()
//      , fourierBsdf.maxCoefficientOrders
//      )
//    , make_span(fourierBsdf.reciprocals)
//    , uniform.x
//    );
//
//  // -- compute scattered direction
//  // the sample incident direction's xy coordinates are given by rotating
//  //   ωₒ.xy by angle ф around the surface normal, and z is given by μᵢ
//  // the xy components are scaled by sinθᵢ/sinθₒ which normalizes the vector.
//  // the computed direction is negated; TODO i probably don't want that not sure
//  auto const wo = surface.incomingAngle;
//  float const
//    sin2ThetaI = glm::max(0.0f, 1.0f - muI*muI)
//  , norm = glm::sqrt(sin2ThetaI / glm::max(0.0f, 1.0f - wo.z*wo.z))
//  , sinPhi = glm::sin(phi), cosPhi = glm::cos(phi);
//
//  glm::vec3 const wi =
//    -glm::vec3(
//      norm * (cosPhi*wo.x - sinPhi*wo.y)
//    , norm * (sinPhi*wo.x + cosPhi*wo.y)
//    , muI
//    );
//
//
//  // -- fourier expansions for angle ф are evaluated
//  // same procedure as the end of BsdfFs (could probably move into fn then)
//  glm::vec3 spectrum;
//
//  float cosScale = muI == 0.0f ? 0.0f : (1.0f/glm::abs(muI));
//  if (transportMode == mt::TransportMode::Radiance && muI*muO > 0.0f) {
//    float const eta = muI > 0.0f ? 1.0f/fourierBsdf.eta : fourierBsdf.eta;
//    cosScale *= eta*eta;
//  }
//
//  switch (fourierBsdf.spectralChannel) {
//    case mt::material::SpectralChannel::Monochromatic:
//      // SPECTRAL NOT IMPLEMENTED YET
//      spectrum = glm::vec3(0.0f);
//    break;
//    case mt::material::SpectralChannel::Rgb:
//      auto const & fourierMaxCoefficients = fourierBsdf.maxCoefficientOrders;
//
//      // calculate RGB channels, green channel can be calculated from luminance
//      glm::vec3 rgb = glm::vec3(0.0f);
//
//      rgb.r =
//        mt::interpolation::Fourier(
//          span<float const>(
//            weightedAlphaK + 1*fourierBsdf.maxCoefficientOrders
//          , maxCoefficients
//          )
//        , cosPhi
//        );
//      rgb.b =
//        mt::interpolation::Fourier(
//          span<float const>(
//            weightedAlphaK + 2*fourierMaxCoefficients
//          , maxCoefficients
//          )
//        , cosPhi
//        );
//
//      // yλ = 0.212671*r + 0.715160*g + 0.072169*b, solved for g
//      rgb.g = 1.39829f*luminance - 0.100913f*rgb.b - 0.297375f*rgb.r;
//
//      // errors in fourier reconstruction can manifest in negative values
//      spectrum = glm::max(glm::vec3(0.0f), rgb*cosScale);
//    break;
//  }
//
//  return { wi, spectrum, glm::max(0.0f, pdfPhi * pdfMu) };
//}
