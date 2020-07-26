#include <monte-toad/material/layered.hpp>

#include <monte-toad/core/geometry.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/math.hpp>
#include <monte-toad/core/span.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <mt-plugin/plugin.hpp>

/*

  A lot of documentation here is most likely just repeated information from the
    following papers

  - "efficient rendering of layered materials using an atomic decomposition
      with statistical operators"

  - "framework for layered materials"

  The premise is that BSDFs can support multiple-layered materials;

     (eye)
        \               /          /
         \             /          /
    ======*===========*==========*==========*================ surface BSDF
           \_       _/ \       _/        __/ \
             \_   _/   ...   _/      ___/    ...
               \ /         _/     __/
    ------------*---------*------*--------------------------- medium BSDF
                 \___    / \_   /
                     \  /    \ /
    ==================*=======*============================== surface BSDF

  Solving this is computationally expensive because of the scattering in
    between each layer. Each surface interaction would essentially result in
    having to solve a one-dimensional light transport equation.

  One observation is that the BSDF can be calculated offline in a large table,
    first the BSDF function is decomposed to represent isotropic spherical
    coordinates;

      f(ωᵢ, ωₒ) = f(μᵢ, фᵢ, μₒ, фₒ)
        where μ is cosine and ф is azimuth angles

  Thus a 4D lookup table of these values can be generated; the problem is the
    amount of storage required for this is highly impractible.

  The BSDF can instead be represented using Fourier series, where a sum of
    scaled cosine terms can efficiently, sparsely, and accurately store
    frequencies that represent the lookup table. It has an added benefit that
    the fourier table can be importance sampled.

  The major cost of this is that each BRDF supported by this system needs to be
    transformed into a fourier sum, either analytically or numerically. The
    fourier series also does not work well with perfectly-specular surfaces,
    but this is not a major problem.

  Another major cost is that it is nearly impossible to use texturing with this
    solution, requires a precomputation step, and can still take large amounts
    of memory to store.

  Instead, it is possible to use directional statistics to fix most of these
    problems. There are three important statistics, energy 'e', mean 'μ' and
    variance 'δ'.

  To compute directional mean and variance, a parametrization is necessary;
    orthographic projection of 3D directions to 2D xy plane. The mean and
    variance is computed with respect to [u, v] = [ωₓ, ωy].


  Listed is a table of different atomic operators and how they can approximate
    eₒ, μₒ, and δₒ given eᵢ, μᵢ, and δᵢ. For each statistic, there is an
    exponent whether it has to be used for reflection or transmission. The
    notation is necessary for adding-doubling method.

  /----------------------------------------------------------------------------\
  | energy   | rough reflection | e^R = eᵢ·FGD^∞                               |
  | ---------+------------------+----------------------------------------------|
  | energy   | rough refraction | e^T = eᵢ·[1 - FGD^∞]                         |
  | ---------+------------------+----------------------------------------------|
  |          |                  |                    - δₜ·h                    |
  | energy   | absorption       | e^T = eᵢ · exp (─────────────)               |
  |          |                  |                  √(1 - |μᵢ|²)                |
  | ---------+------------------+----------------------------------------------|
  |          |                  |               σₛ·h               - σₜ·h      |
  | energy   | fwd scattering   | e^T = eᵢ· ―――――――――――― · exp (─────────────) |
  |          |                  |           √(1 - |μᵢ|²)         √(1 - |μᵢ|²)  |
  | ---------+------------------+----------------------------------------------|
  | mean     | rough reflection | μ^R = -μᵢ                                    |
  | ---------+------------------+----------------------------------------------|
  | mean     | rough refraction | μ^T = -η₁₂ · μᵢ                              |
  | ---------+------------------+----------------------------------------------|
  | mean     | absorption       | μ^T = -μᵢ                                    |
  | ---------+------------------+----------------------------------------------|
  | mean     | fwd scattering   | μ^T = -μᵢ                                    |
  | ---------+------------------+----------------------------------------------|
  | variance | rough reflection | σ^R = σᵢ + σ₁₂^R                             |
  | ---------+------------------+----------------------------------------------|
  |          |                  |         σᵢ                                   |
  | variance | rough refraction | σ^T = ――――― + f(s · α₁₂)                     |
  |          |                  |        η₁₂                                   |
  | ---------+------------------+----------------------------------------------|
  | variance | absorption       | σ^T = σᵢ                                     |
  | ---------+------------------+----------------------------------------------|
  | variance | fwd scattering   | σ^T = σᵢ + σg                                |
  \----------+------------------+----------------------------------------------/

                       η₁
    where  η₁₂ is IOR, ――
                       η₂
  ------------------------------------------------------------------------------
  --- derivation of atomic operators -------------------------------------------
  ------------------------------------------------------------------------------

  --- rough reflection ---------------------------------------------------------

  The microfacet BRDF can be described as

             ⌠         G(ωᵢ,ωₒ)·D(h)
    Lₒ(ωₒ) = │ F(h∘n)·───────────────·Lᵢ(ωᵢ)·(ωᵢ∘n) dωᵢ
             ⌡Ω       4·(ωᵢ·n)·(ωₒ·n)

      where h is half vector, G is shadowing/masking function, D is
        distribution function, and F is fresnel.


  --- computing energy

  The amount of energy reflected by surface is the directional albedo. It is
    defined by the integrated Fresnel FGD

                       ⌠         G(ωᵢ, ωₒ)·D(h)
    FGD(ωᵢ, α, η+iₖ) = │ F(h∘n) ─────────────── · (ωᵢ∘n) dωₒ
                       ⌡Ω       4·(ωᵢ∘n)·(ωₒ∘n)

      where η + iₖ is the complex IOR at the interface.

  The radiance can then be approximated by decoupling the FGD from incident
    radiance

    ⌠                  ⌠
    │ Lₒ(ωₒ) dωₒ ≃ FGD·│ Lᵢ(ωᵢ) dωᵢ
    ⌡Ω                 ⌡Ω

  since no closed form exists for FGD, it is precomputed into a 4D table
    parametrized by an elevation, roughness and complex IOR.

    e^T = eᵢ·FGD^∞

  --- computing mean

  The mean of a reflected lobe shifts towards the normal; this is typically
    to fetch preintegrated environment maps. The shift is directly integrated
    into the FGD texture; a good approximation of this shift is

    ω'.xy = β·n.xy + (1 - β)·ω.xy

      where ω.xy is the vector in the tangential plane, and β depends on the
        shadowing-mask term.

  This shift is only important for rough configurations, and neglected during
    estimation of the BRDF. TODO probably don't want to do this though right?

    μ^R = -μᵢ

  --- computing variance

  A rough surface increses variance of the incident distribution. However,
    this increase is not linear with respect to roughness α < 0.2. For higher
    roughnesses, a space is found where rough reflections have linear behaviour.

  To find function f(α) that transforms roughness of GGX lobe into a space
    where multiple bounces are linear, start from a 2-bounce case where both
    surfaces have same roughness;

    α₁₂ = f(α₁) + f(α₁)
        = f⁻¹(f(α) + f(α))
        = f⁻¹(2·f(α))

      where f(α) is the transform of roughness to variance

  thus

    f(α₁₂) = 2.0f·f(α)

  To extract α₁₂ as a function of α, invert numerically the function variance
    for roughness for the 1-bounce case and evaluate the roughness corresponding
    to 2-bounces variances.

  Specifically, the linear space transformation can be extracted to a scaling
    factor. The f(α₁₂) is iterated for f(α) and evaluated for α₁₂ using current
    function value.

  Roughness can be converted to linear variance using

                α¹⋅¹
    σ = f(α) ≈ ──────
               1-α¹⋅¹

  from linear variance to roughness using the inverse. The transform does not
    perfectly fit linear-space mapping, but it approximates the outgoing
    directional statistics of two light bounces on microfacet interfaces of
    moderate roughnesses (below 0.4)

    σ^T = σᵢ + σ₁₂^R

  --- misc

  Classic microfacet does not conserve energy from multiple in-scattering
    events. however, a scaling factor approximates this well. It is
    incorporated directly into the FGD term during precomputation

  Thus these terms are approximated by the rough reflection terms in the table

  --- rough refraction ---------------------------------------------------------

  A rough dielectric interfaces distributes light in lower hemisphere of local
    direction as BTDF.

  --- computing energy

  Energy scaling of an incident angular field by rough dielectic interface is
    given by 1 - FGD∞. This also ensures energy conservation between reflected
    and refracted directional fields.

  e^T = eᵢ·(1-FGD∞)

  --- computing mean

  For rough interfaces, the transmitted lobe peak is not the refracted incident
    direction. Mean refracted direction is roughness dependent. Direction also
    depends on the refractive index. At grazing angles, for higher refractive
    IORs, the peak of the lobe differs mokre from the refracted incident
    direction. Using the refracted direction was sufficient.

    μ^T = -η₁₂ · μᵢ

  --- computing variance

  Due to Snell's law, light rays bend when passing through a dielectric
    interface. The incident variance is scaled by the IOR transmittance, and
    incrased by the roughness of the surface. For the same roughness and
    incident directions, reflected and transmitted lobes have different widths.
    Transmitted roughness is used to analyze the variance of the transmitted
    lobe. Have to fake the transmitted lobe using a reflection from underneath
    the interface.

  To match transmitted lobe, the roughness is scaled. Scale s is the ratio of
    derivatives of the transmitted and reflected half vector tangent;

        1 ⎛          n∘ωᵢ⎞
    s = ―·⎜1 + η₁₂ · ────⎟
        2 ⎝          n∘ωₜ⎠

    thus,

           σᵢ
    σ^T = ─── + f(s · α₁₂)
          η₁₂

  --- volume absorption---------------------------------------------------------

  Calculates changes in energy/mean/variance from participating media. This
    covers how absorption affects statistics.

  --- computing energy

  The attenuation of the incident light-field by a rnadom medium is described
    by Beer-Lambert's law. It states that incident radiance is scaled down
    depending on the optical depth;

                ⎛     h  ⎞
    Lₒ(ωₒ) = exp⎜-σₜ·────⎟ * Lᵢ(ωₒ)
                ⎝    ωᵢ∘n⎠

  where δₜ is the transmittance cross-section and h is the depth of the layer.
    The average energy is approximated using the incident mean attenuation;

                 ⎛ σₜ·h⎞
    eₒ = eᵢ * exp⎜-────⎟
                 ⎝ ωₒ∘n⎠

  thus,
                  ⎛    σₜ·h   ⎞
    e^T = eᵢ · exp⎜-──────────⎟
                  ⎝ √(1-|μᵢ|²)⎠

  --- computing mean and variance

    The impact of absorption on the mean and variance is negligible.

    μ^T = -μᵢ
    σ^R = σᵢ

  --- volume scattering --------------------------------------------------------

  This describes an optically thin homogeneous slab of height h that does not
    emit light. Single scattering is predominant and multiple scattering can be
    neglected. Given the light incident to slab Lᵢ(ω), the amount of light
    scattered by the medium is;

               ⌠h   ⎛ σₜ · t⎞
               │ exp⎜-──────⎟ · Lₛ(t, ω) dt
    Lₒ(ω) = σₛ ⌡0   ⎝  ω∘n  ⎠

  where

                ⌠              ⎛ σₜ·(h - t)⎞
    Lₛ(t, ω) =  │  p(ωᵢ, ω) exp⎜-──────────⎟ · Lᵢ(ωᵢ) dωᵢ
                ⌡S²            ⎝   ωᵢ∘n    ⎠

  and σₛ is the scattering cross section

  --- computing energy and mean

  If the phase function is strongly forward scattering, the outgoing energy can
    be approximated using attenuation evaluated in the mean direction of the
    incoming light;

    ⌠                      ⎛ σₜ·h⎞   ⌠
    │ Lₒ(ω) dω  ≈ σₛ·h·exp ⎜-────⎟ · │ Lᵢ(ω)dω
    ⌡Ω                     ⎝ ωₒ∘n⎠   ⌡Ω

  This approximation assumes that the phase function does not lose energy in
    backwards direction. It is possible to account for backscattering by
    modulating the energy by the amount of light the phase function scatters
    forward. For g ≥ 0.7, scaling is unnecessary. Absorption does not alter
    mean significantly for forward phase functions.

  thus,
             σₛ·h    ⎛ σₜ·h⎞
    e^T = eᵢ·────·exp⎜-────⎟
             ωₒ∘n    ⎝ ωₒ∘n⎠

    μ^T = -μᵢ

  --- computing variance

  For rough interfaces, the incident variance increases by width of the phase
    function in the forward direction. GGX roughness for the Henyey-Greenstein
    phase function in the forward direction is;

          ⎛1-g⎞⁰⋅⁸    1
    σHG = ⎜───⎟   · ─────
          ⎝ g ⎠     1 + g

    where g is HG anisotropy factor.

  The HG phase function can have a non-negligible backscattering when g « 0.7.
    The derivation of the backscattering variance was left out of the paper and
    needs to be calculated

  thus,

    σ^R = σᵢ + σHG

  ------------------------------------------------------------------------------
  --- statistics with multiple layers ------------------------------------------
  ------------------------------------------------------------------------------

  Need to combine statistics of path groups (ei TRT, TTRT, etc) paths in an
    efficient method. The adding-doubling algorithm is used for this, and is
    applied to each energy and variance statistics; the mean approximately
    aligns with reflected/refracted directions and does not need to be treated.

  --- adding/doubling method ---------------------------------------------------

  The adding-doubling method allows discrete expression of radiative transfer
    in plane parallel media. Thin homogenous slabs with linear operators are
    combined to estimate transmission/reflection of a thick heterogeneous slab;

    l⁺ₒ = r₁₂·l⁻ᵢ + t₂₁·l⁺ᵢ

    where,
      r is reflection coefficient and t is transmission radiance coefficient
      rₖ ₖ₊₁ is the reflection coefficient for downwards light propagation
      rₖ₊₁ ₖ is the reflection coefficient for upwards light propagation

  Thus, light propagating upwards is the reflected incident light propagating
    downward, plus the transmitted incident light.

  For light propagating downwards the same logic follows;

    l⁻ₒ = r₂₁·l⁺ᵢ + t₁₂·l⁻ᵢ

        ┌──────────────────────────────┬─────────────────────────────┐
        │                              │                             │
        │                              │      .        .    .        │
        │        .           . .       │   l⁻ᵢ ╲   r₁₃╱    ╱ t₁₃     │
        │     l⁻ᵢ ╲      r₁₂╱ ╱ t₁₂    │        ╲    ╱    ╱          │
        │          ╲       ╱ ╱         │         ╲  ╱    ╱           │
        │           ╲     ╱ ╱          │  η₁      ╲╱    ╱            │
        │ η₁         ╲   ╱ ╱           │  ┏━━━━━━━━━━━━━━━━━━━━━━━━┓ │
        │ ┏━━━━━━━━━━━╲━╱━╱━━━━━━━━┓   │  ┗━━━━━━━━━━━━━━━━━━━━━━━━┛ │
        │ ┃            * *         ┃   │  η₂                         │
        │ ┗━━━━━━━━━━━━━╳━╲━━━━━━━━┛   │  ┏━━━━━━━━━━━━━━━━━━━━━━━━┓ │
        │ η₂           ╱ ╲ ╲           │  ┗━━━━━━━━━━━━━━━━━━━━━━━━┛ │
        │             ╱   ╲ ╲          │  η₃       ╲ ╱╲              │
        │        l⁺ᵢ ⋅     ⋅ ⋅ r₂₁     │            ╳  ╲             │
        │                 t₁₂          │           ╱ ╲  ╲            │
        │                              │      l⁺ᵢ ⋅   ⋅  ⋅ r₃₁       │
        │                              │              t₁₃            │
        │ transmittance/reflectance of │  adding-doubling scatters   │
        │ a thick heterogenous slab    │  thin homogeneous slabs,    │
        │                              │  allowing inter-reflections │
        └──────────────────────────────┴─────────────────────────────┘

  The global upward reflectance between layers r₁₂ and r₂₃ is expressed as

    r₁₃ = r₁₂ + t₁₂·r₂₃·t₂₁ + t₁₂·r²₂₃·r₂₁·t₂₁ + ···

                ∞
        = r₁₂ + Σ t₁₂·rᵏ⁺¹₂₃·rᵏ₂₁·t₂₁
                ᵏ

  This accounts for all bounces between two layers. It can be summarized in
    analytical form;

                t₁₂·r₂₃·t₂₁
    r₁₃ = r₁₂ + ───────────
                1 - r₂₃·r₂₁

  The rest of the upward/downward reflectance/transmittance can be described
    with the same logic;

            t₃₂·t₂₁
    t₃₁ = ───────────
          1 - r₂₃·r₂₁

                t₃₂·r₂₁·t₂₃
    r₃₁ = r₃₂ + ───────────
                1 - r₂₃·r₂₁

            t₁₂·t₂₃
    t₁₃ = ───────────
          1 - r₂₃·r₂₁

  The adding algorithm can thus express multiple interface by using r₁₃, t₁₃,
    r₃₁, t₃₁.

  Multiple scattering can be evaluated in a homogeneous medium using the
    doubling algorithm. The layers are stacked together to generate a lacker
    twice the original depth;

    r₁₂ = r₂₃ | t₁₂ = t₂₃ | r₂₁ = r₃₂ | t₂₁ = t₃₂

  By iterating this operation on an input layer of small size (h ≈ 10⁻⁸), the
    depth of a layer can be increased cheaply to any size.

  The r and t terms in adding/doubling can now be replaced with the previously
    described atomic operators.

  For example, with a rough surface;

    rough reflection | e^R = eᵢ·FGD^∞       | r₁₂ = FGD      | r₂₁ = FGD
    rough refraction | e^T = eᵢ·(1 - FGD^∞) | t₁₂ = 1 - FGD^∞| t₂₁ = 1 - FGD^∞

  --- total internal reflection ------------------------------------------------

  Adding/doubling approximates multiple scattering between two layers, it does
    not compute totally reflected light by the upper Fresnel interfaces; no
    light is refracted and the dielectric interface behaves as a specular
    surface. t₂₁ does not account for the angular spread of light reflected on
    interface 2 ⇒ 3. To compute this the following is necessary;


    Lₜ(ωₒ) =

        ⌠
        │ Lᵢ(ωᵢ)·p(ωᵢ, ωₜ, α₂₃) dωₜ
        ⌡Ω

        ⌠                            ⎫
     ·  │ (1 - F(ωₜ))·D(ωₜ, α₂₃) dωᵢ ⎬ total internal reflection
        ⌡Ω                           ⎭

    where,
      ωₜ = refract(ωₒ, η₁₂)
      T(ωₜ) = 1 - F(ωₜ) ⎬ Fresnel transmittance
      p(ωᵢ, ωₜ, α₂₃) is the microfacet BRDF of 2 ⇒ 3
      D(ωᵢ, α₂₃) is the microfacet NDF of 2 ⇒ 3

  When TIR occurs, T(ωᵢ) = 0 for ωᵢ outside of the extinction cone. The second
    integral is similar to the FGD table, so it is precomputed in a 3D table.

  During adding-doubling, r₂₁ and t₂₁ can be replaced by

    r₂₁ ← r₂₁ + (1 - TIR)·t₂₁
    t₂₁ ← TIR · t₂₁

  --- adding-doubling for variance ---------------------------------------------

  Given two interfaces, the unnormalized average variance ~σ^R₁₃ accounting for
    multiple scattering is;

    ~σ^R₁₃ =

        r₁₂ · σ^R₁₂

         ∞  ⎛                   ⎞ ⎛        ⎛                     ⎞            ⎞
      +  Σ  ⎜t₁₂·rᵏ⁺¹₂₃·rᵏ₂₁·t₂₁⎟·⎜σ^T₁₂ + ⎜(k+1)·s^R₂₃ + k·σ^R₂₁⎟·J₂₁ + σ^T₂₁⎟
        k=0 ⎝                   ⎠ ⎝        ⎝                     ⎠            ⎠

    where,

      s^Rᵢₖ is the additional variance when reflecting on a rough interface
        between indices of refraction i and k.

      Jᵢₖ is the transmission scaling factor.

  The formula can be seperated into a geometric series and
    arithmetico-geometric series. The latter has the following analytical form;

      ∞           r
      Σ k·rᵏ = ────────     with r ∈ [0, 1]
     k=0       (1 - r)²

  Since r₂₃·ᵣ₂₁ < 1, the unnormalized average variance equation can be
    simplified to;

    ~σ^R₁₃ =

      r₁₂·σ^R₁₂

       ⎛ t₁₂·r₂₃·t₂₁ ⎞   ⎛                    ⎛          r₂₃·r₂₁        ⎞⎞
     + ⎜─────────────⎟ · ⎜σ^T₁₂ + σ^T₂₁ + J₂₁·⎜σ^R₂₃ + ───────────·σ^R₂₁⎟⎟
       ⎝ 1 - r₂₃·r₂₁ ⎠   ⎝                    ⎝        1 - r₂₃·r₂₁      ⎠⎠

  Similar forms can be derived for transmittance average variance;

  ~σ^R₃₁ =

      r₃₂·s^R₃₂

       ⎛ t₃₂·t₂₃·r₂₁ ⎞
     + ⎜─────────────⎟
       ⎝ 1 - r₂₁·r₂₃ ⎠

       ⎛            ⎛                                ⎛   r₂₁·r₂₃   ⎞⎞⎞
     · ⎜s^T₂₃ + J₂₃·⎜s^T₃₂ + σ^R₂₁ + (s^R₂₃ + σ^R₂₁)·⎜─────────────⎟⎟⎟
       ⎝            ⎝                                ⎝ 1 - r₂₁·r₂₃ ⎠⎠⎠

  When computing intermediate variances, global scaling factor J₁ᵢ and ᵢ₁ will
    have to be kept track of, due to all interfaces from top layer to the
    current bototm layer.

  Transmittance average variance will be used in the next iteration of the
    adding algorithm in place of σ^R₁₂ and σ^T₁₂.

  For participating media;

      σ^R₁₂ = 0
      σ^R₂₁ = 0
      σ^T₁₂ = σHG
      J₁₂ = 1
      J₂₁ = 1

  --- adding-doubling algorithm ------------------------------------------------

  Using multiple scattering equations, an adding-doubling algorithm can be
    built. Starting with an empty layer;

    e^Rᵢₖ = e^Rₖᵢ = 0
    e^Tᵢₖ = e^Tₖᵢ = 1
    σ^Rᵢₖ = σ^Rₖᵢ = σ^Tᵢₖ = σ^Tₖᵢ = 0

  For each interface in the stack, going from top layer to bottom, the multiple
    scattering equations on energies and variances are applied. The refraction
    scaling factors Jᵢₖ and Jₖᵢ will need to be kept track of during the
    process.
*/

/*
  To evaluate the BSDF, start with

    eᵢ = 1
    μᵢ = [ωᵢ.x, ωᵢ.y[
    σᵢ = 0


  and use adding-doubling to gather intermediate variance and energy in a
    vector of BRDF lobes [ComputeAddingAndDoubling]. For transmitted lobe, the
    transmitted statistics are recorded at the end of the adding-doubling
    algorithm;

      coeffs[0] = R0i;
      dirs[0]   = wi;
      alphas[0] = varianceToRoughness(s_r0i);
      coeffs[1] = T0i;
      dirs[1]   = reflect(wt, N);
      alphas[1] = varianceToRoughness(s_t0i);

  Each entry in the vector corresponds to a BSDF lobe and contains its energy
    eₖ, mean μₖ and variance σₖ. From this vector of statistics, an approximate
    BSDF model can be instantiated consisting of a weighted sum of microfacet
    GGX lobes with fresnel eₖ, incident direction reflect(μₖ), and roughness
    f⁻¹(σₖ);

                N
    p(ωᵢ, ωₒ) = Σ eₖ·pₖ(ωₖ, ωₒ, αₖ)
               k=0

    with,
      αₖ = f⁻¹(σₖ)
      ωₖ = reflect(μₖ)
                       D(h)·G(ωₖ, ωₒ)
      pₖ(ωₖ, ωₒ, αₖ) = ───────────────
                       4·(ωₖ∘n)·(ωₒ∘n)

      eₖ, ωₖ and σₖ are the energy, mean and variance for the k-th lobe.


  -- importance sampling

  Since model is a weighted average of multiple lobes, one os randomly selected
    based on the neergy and importance sample of the visible normals based on
    the incident direction and roughness. To avoid overlapping from lobes, MIS
    is applied on the different lobes with a balance heuristic, thus the
    contribution of a BRDF lobe sample becomes

         eₐₗₗ
    p = ─────── · Σ eᵢ·pᵢ(ωᵢ, ωₒ, αᵢ)
        Σ eᵢ·Pᵢ

    where,

      eᵢ is the energy for the i-th lobe
      eₐₗₗ is total energy
      Pᵢ is the PDF of sampling the vNDF for roughness αᵢ
      pᵢ is the pdf of the i-th microfacet model

*/

namespace {

float RoughnessToVariance(float a) {
  // TODO there is a 'best fit' option
  return a/(1.0f-a);
}

struct LayeredMoment {
  std::vector<glm::vec3> coefficients;
  std::vector<float> alphas;
};

std::tuple<glm::vec3, glm::vec3>
EvalFresnel(
  mt::material::layered::Data const & structure
, float ct, float alpha, glm::vec3 const & eta, glm::vec3 const & kappa
) {
  glm::vec3 rij = structure.fresnelTable.Get(ct, alpha, eta, kappa);
  glm::vec3 tij =
    kappa == glm::vec3(0.0f) ? glm::vec3(1.0f) - rij : glm::vec3(0.0f);

  /* spdlog::info("rij {} tij {}", rij, tij); */

  return { rij, tij };
}

// moment analysis in layer structure for a given input angle
LayeredMoment ComputeAddingDoubling(
  mt::material::layered::Data const & structure
, float const cti
, span<float    > const alphas
, span<glm::vec3> const etas
, span<glm::vec3> const kappas
) {
  LayeredMoment moment;

  // R = reflection, T = transmission
  // S = variance (σ)
  // J = jacobian
  // m = mean (μ)
  // e = energy (e)

  glm::vec3 itR0i, itRi0, itT0i, itTi0;
  itR0i = itRi0 = glm::vec3(0.0f);
  itT0i = itTi0 = glm::vec3(1.0f);

  float itSR0i, itSRi0, itST0i, itSTi0, itJ0i, itJi0;
  itSR0i = itSRi0 = itST0i = itSTi0 = 0.0f;
  itJ0i = itJi0 = 1.0f;
  auto itCti = cti;

  moment.coefficients.resize(structure.layers.size());
  moment.alphas      .resize(structure.layers.size());

  assert(etas.size()-1   == structure.layers.size());
  assert(kappas.size()-1 == structure.layers.size());
  assert(alphas.size()   == structure.layers.size());

  for (size_t i = 0; i < structure.layers.size(); ++ i) {
    // extract layer data
    auto & layer = structure.layers[i];
    glm::vec3 const
      eta1   = etas[i]
    , eta2   = etas[1]
    , kappa2 = kappas[i+1]
    , eta    = eta2/eta1
    , kappa  = kappa2 / eta1;

    float const
      alpha = alphas[i]
    , n12   = (eta.x + eta.y + eta.z) * 0.33333333f
    , depth = layer.depth;

    glm::vec3 r12, t12, r21, t21;
    float sr12, sr21, st12, st21, j12, j21, ctt;

    r12 = t12 = r21 = t21 = glm::vec3(0.0f);
    sr12 = sr21 = st12 = st21 = 0.0f;
    j12 = j21 = 1.0f;

    if (depth > 0.0f) {
      // mean doesn't change with volumes
      ctt = itCti;

      // evaluate transmittance
      glm::vec3 const sigmaT = glm::vec3(layer.sigmaA + layer.sigmaS);
      float const rayLength = depth/ctt;
      t12 = (glm::vec3(1.0f) + layer.sigmaS*rayLength) * (-rayLength*sigmaT);
      t21 = t12;
      r12 = glm::vec3(0.0f);
      r21 = glm::vec3(0.0f);

      // fetch precomputed variance for HG phase function
      st12 = st21 = alpha;
    } else {
      // evaluate off-specular transmission
      float const
        sti = glm::sqrt(1.0f - itCti*itCti)
      , stt = sti/n12;

      ctt = (stt <= 1.0f ? glm::sqrt(1.0f - stt*stt) : -1.0f);

      // check if ray is being blocked by a conducting interface or total
      // reflection
      bool const hasTransmission = ctt > 0.0f && kappa == glm::vec3(0.0f);

      // evaluate interface variance term
      sr12 = sr21 = RoughnessToVariance(alpha);

      // for dielectric interfaces, evaluate transmissive roughness
      if (hasTransmission) {
        // scaling factor overblurs BSDF at grazing angles (cannot account for
        // deformation of lobe for thse configurations)
        const float invN12 = 1.0f/n12;
        st12 = RoughnessToVariance(alpha*0.5f * glm::abs(n12    - 1.0f)/n12);
        st21 = RoughnessToVariance(alpha*0.5f * glm::abs(invN12 - 1.0f)/invN12);

        // scale due to interface
        j12 = (ctt/itCti) * n12;
        j21 = (itCti/ctt) / n12;
      }

      // evaluate FGD using modified rougness accounting for top layers
      auto tempAlpha = RoughnessToVariance(itST0i + sr12);

      // evaluate r12, r21, t12, r21
      std::tie(r12, t12) = EvalFresnel(structure, itCti, tempAlpha, eta, kappa);

      if (hasTransmission) {
        r21 = r12;
        t21 = t12; // TODO probably have to account for IOR * (n12*n12)
        /* t12 = t12; // same as above */
      } else {
        r21 = t21 = t12 = glm::vec3(0.0f);
      }

      // evaluate TIR using decoupling approximation
      if (i > 0) {
        glm::vec3 eta0 = etas[i-1];
        float const n10 = glm::dot(glm::vec3(0.3333333f), eta0/eta1);

        float const tir =
            structure.tirTable.Get(glm::vec3(itCti, tempAlpha, n10));
        itRi0 = glm::min(glm::vec3(1.0f), itRi0 + (1.0f - tir) * itTi0);
        itTi0 *= tir;
      }
    }

    // evaluate adding operators on energy
    glm::vec3 const denom = (glm::vec3(1.0f) - itRi0*r12);

    // multiple scattering forms
    float const denomAvg = glm::dot(glm::vec3(0.33333333f), denom);
    glm::vec3 const
      mR0i = denomAvg > 0.0f ? (itT0i*itTi0*r12)/denom : glm::vec3(0.0f)
    , mRi0 = denomAvg > 0.0f ? (t21*itRi0*t12)  /denom : glm::vec3(0.0f)
    , mRr  = denomAvg > 0.0f ? (itRi0*r12)      /denom : glm::vec3(0.0f)
    ;

    /* spdlog::info( */
    /*   "mR0i {} denomAvg {} itT0i {} r12 {} denom {} itRi0 {}", */
    /*   mR0i, denomAvg, itT0i, r12, denom, itRi0 */ 
    /* ); */

    float const
      mR0iAvg = glm::dot(glm::vec3(0.3333333f), mR0i)
    , mRi0Avg = glm::dot(glm::vec3(0.3333333f), mRi0)
    , mRrAvg  = glm::dot(glm::vec3(0.3333333f), mRr)
    ;

    // evaluate adding operator on energy
    glm::vec3 const
      eR0i = itR0i + mR0i
    , eT0i = (itT0i*t12)/denom
    , eRi0 = r21 + mRi0
    , eTi0 = (t21*itTi0)/denom
    ;

    // scalar energy forms to compute adding on variance
    float const
      r21Avg   = glm::dot(glm::vec3(0.3333333f), r21)
    , itR0iAvg = glm::dot(glm::vec3(0.3333333f), itR0i)
    , eR0iAvg  = glm::dot(glm::vec3(0.3333333f), eR0i)
    , eRi0Avg  = glm::dot(glm::vec3(0.3333333f), eRi0)
    ;

    // evaluate adding operator on the normalized variance
    float
      sR0i =
        itR0iAvg*itSR0i
      + mR0iAvg*(itSTi0 + itJ0i*(itST0i + sr12 + mRrAvg*(sr12+itSRi0)))
    , sT0i = j12*itST0i + st12 + j12*(sr12 + itSRi0)*mRrAvg
    , sRi0 =
        r21Avg*sr21
      + mRi0Avg*(st12 + j12*(st21 + itSRi0 + mRrAvg*(sr12+itSRi0)))
    , sTi0 = itJi0*st21 + itSTi0 + itJi0*(sr12 + itSRi0)*mRrAvg
    ;

    sR0i = eR0iAvg <= 0.0f ? sR0i/eR0iAvg : 0.0f;
    sRi0 = eRi0Avg <= 0.0f ? sRi0/eRi0Avg : 0.0f;

    // store coefficient and variance
    /* spdlog::info("mR0iAvg {}", mR0iAvg); */
    if (mR0iAvg > 0.0f) {
      moment.coefficients[i] = mR0i;
      moment.alphas[i] =
        RoughnessToVariance(
          itSTi0 + itJ0i*(itST0i + sr12 + mRrAvg*(sr12+itSRi0))
        );
    } else {
      moment.coefficients[i] = glm::vec3(0.0f);
      moment.alphas[i] = 0.0f;
    }

    // update energy
    itR0i = eR0i;
    itT0i = eT0i;
    itRi0 = eRi0;
    itTi0 = eTi0;

    // update mean
    itCti = ctt;

    // update variance
    itSR0i = sR0i;
    itST0i = sT0i;
    itSRi0 = sRi0;
    itSTi0 = sTi0;

    // update jacobian
    itJ0i *= j12;
    itJi0 *= j21;

    // if a conductor is present, exit
    if (glm::dot(glm::vec3(0.33333333f), kappa) > 0.0f) {
      moment.alphas.resize(i+1);
      moment.coefficients.resize(i+1);
      return moment;
    }
  }

  return moment;
}

std::tuple<
  std::vector<float>     /* alphas */
, std::vector<glm::vec3> /* etas */
, std::vector<glm::vec3> /* kappas */
> EvaluateTextureMaps(
  mt::material::layered::Data const & structure
, mt::core::SurfaceInfo const & /*surface*/
) {
  std::vector<float>     alphas; alphas.resize(structure.layers.size());
  std::vector<glm::vec3> etas;   etas.resize(structure.layers.size()+1);
  std::vector<glm::vec3> kappas; kappas.resize(structure.layers.size()+1);

  // fetch air values
  etas[0] = structure.layers[0].ior; // TODO sample from texture
  kappas[0] = structure.layers[0].kappa; // TODO sample from textures

  // fetch textured values
  for (size_t i = 0; i < structure.layers.size(); ++ i) {
    /* etas[i+1] = glm::vec3(1.3f); */
    /* kappas[i+1] = glm::vec3(0.5f); */
    etas[i] = structure.layers[i].ior;
    kappas[i] = structure.layers[i].kappa;
    alphas[i] = structure.layers[i].alpha;
  }

  return { alphas, etas, kappas };
}

glm::vec3 Evaluate(
  mt::material::layered::Data const & structure
, mt::core::SurfaceInfo const & surface
, glm::vec3 const & wo
, span<float    > const alphas
, span<glm::vec3> const etas
, span<glm::vec3> const kappas
) {
  glm::vec3 const wi = -surface.incomingAngle;
  glm::vec3 const h = normalize(wo+wi);

  auto bsdfValue = glm::vec3(0.0f);

  // evaluate adding method to get coefficients and variances
  LayeredMoment moment =
    ComputeAddingDoubling(structure, glm::dot(wi, h), alphas, etas, kappas);

  // sum contribution of all interfaces
  for (size_t it = 0; it < moment.alphas.size(); ++ it) {
    // skip zero contribution
    if (moment.coefficients[it] == glm::vec3(0.0f)) { continue; }

    /* // fetch roughness */
    /* float const a = alphas[it]; */

    // evaluate microfacet model

    // GGX distribution TODO
    bsdfValue +=
      moment.coefficients[it] / (4.0f * glm::dot(surface.normal, wi));

    /* spdlog::info( */
    /*   "{} coefficient {} bsdfValue {} dot {}", */
    /*   it, moment.coefficients[it], bsdfValue, glm::dot(surface.normal, wi) */
    /* ); */
  }
  return bsdfValue;
}

} // -- namescape

float mt::material::layered::FresnelMicrofacetTable::Get(
  glm::vec4 const & idx
) const {
  const glm::vec4 flIdx = glm::vec4(size) * (idx - min)/(max - min);
  const glm::uvec4 intIdx =
    glm::clamp(glm::uvec4(flIdx), glm::uvec4(0), size-glm::uvec4(1));

  const uint32_t bufferIdx =
    intIdx.w + size.w*(intIdx.z + size.z*(intIdx.y + size.y*intIdx.x));

  // TODO linear interpolation

  // non-linear interpolation
  return this->buffer[bufferIdx ];
}

glm::vec3 mt::material::layered::FresnelMicrofacetTable::Get(
  float t
, float a
, glm::vec3 const & n
, glm::vec3 const & k
) const {
  if (this->buffer.size() == 0) { return this->fresnelConstant; }
  return glm::vec3(1.0f);
  return
    glm::vec3(
      this->Get(glm::vec4(t, a, n.x, k.x))
    , this->Get(glm::vec4(t, a, n.y, k.y))
    , this->Get(glm::vec4(t, a, n.z, k.z))
    );
}

float mt::material::layered::TotalInternalReflectionTable::Get(
  glm::vec3 const & idx
) const {
  if (this->buffer.size() == 0) { return this->tirConstant; }

  // grab floating point idx
  const glm::vec3 flIdx = glm::vec3(size) * (idx - min)/(max - min);
  const glm::uvec3 intIdx =
    glm::clamp(glm::uvec3(flIdx), glm::uvec3(0), size-glm::uvec3(1));

  const uint32_t bufferIdx = intIdx.z + size.z*(intIdx.y + size.y*intIdx.x);

  // TODO linear interpolation
  /* // get interpolation weights */
  /* const glm::vec3 alphas = glm::saturate(flIdx - glm::vec3(intIdx)); */
  /* // index of middle point */
  /* glm::uvec3 middleIndices = intIdx; */

  // non-linear interpolation
  return this->buffer[bufferIdx];
}

float mt::material::layered::BsdfPdf(
  mt::material::layered::Data const & structure
, mt::core::SurfaceInfo const & surface
, glm::vec3 const & /*wo*/
) {
  auto const wi = -surface.incomingAngle;
  /* glm::vec3 const h = glm::normalize(wo + wi); */

  auto [alphas, etas, kappas] = EvaluateTextureMaps(structure, surface);

  /* for (size_t i = 0; i < alphas.size(); ++ i) { */
  /*   spdlog::info( */
  /*     "{}: alpha {} eta {} kappa {}", */
  /*     i, alphas[i], etas[i], kappas[i] */
  /*   ); */
  /* } */

  LayeredMoment moment =
    ComputeAddingDoubling(
      structure, glm::dot(wi, surface.normal)
    , make_span(alphas), make_span(etas), make_span(kappas)
    );

  /* spdlog::info("Alphas {}", moment.alphas.size()); */

  // convert spectral coefficients to float for pdf weighting
  float pdf = 0.0f, cumulativeW = 0.0f;
  for (size_t i = 0; i < moment.alphas.size(); ++ i) {

    /* spdlog::info("{} alpha: {}", i, moment.alphas[i]); */
    /* spdlog::info("{} coeff: {}", i, moment.coefficients[i]); */

    // skip zero contribs
    if (moment.coefficients[i] == glm::vec3(0.0f)) { continue; }

    // evaluate weight
    float const weight = glm::dot(glm::vec3(0.333333f), moment.coefficients[i]);
    cumulativeW += weight;

    // evaluate PDF
    float const microfacetPdf = 1.0f/(4.0f * glm::dot(wi, surface.normal));
    pdf += weight * microfacetPdf;
  }

  /* spdlog::info("pdf {} cumulativeW {}", pdf, cumulativeW); */

  return cumulativeW > 0.0f ? pdf/cumulativeW : 0.0f;
}


glm::vec3 mt::material::layered::BsdfFs(
  mt::material::layered::Data const & structure
, mt::core::SurfaceInfo const & surface
, glm::vec3 const & wo
) {
  auto [alphas, etas, kappas] = EvaluateTextureMaps(structure, surface);

  auto result =
    Evaluate(
      structure, surface, wo
    , make_span(alphas), make_span(etas), make_span(kappas)
    );
  assert(!glm::isnan(result));
  return result;
}

std::tuple<glm::vec3 /*wo*/, glm::vec3 /*bsdfFs*/, float /*pdf*/>
mt::material::layered::BsdfSample(
  mt::material::layered::Data const & structure
, mt::PluginInfoRandom const & random
, mt::core::SurfaceInfo const & surface
) {
  auto const wi = -surface.incomingAngle;

  auto [alphas, etas, kappas] = EvaluateTextureMaps(structure, surface);
  LayeredMoment moment =
    ComputeAddingDoubling(
      structure, glm::dot(wi, surface.normal)
    , make_span(alphas), make_span(etas), make_span(kappas)
    );

  // convert spectral coefficients to floats to select BRDF lobe to sample
  std::vector<float> weights; weights.resize(moment.alphas.size());
  float cumulativeW = 0.0f;
  for (size_t i = 0; i < weights.size(); ++ i) {
    weights[i] = glm::dot(glm::vec3(0.33333333f), moment.coefficients[i]);
    cumulativeW += weights[i];
  }

  // select a random BSDF lobe
  float selectW = random.SampleUniform1() * cumulativeW - weights[0];
  size_t selectIdx = 0;
  for (; selectW > 0.0f && selectIdx < moment.alphas.size()-1; ++ selectIdx) {
    selectW -= weights[selectIdx+1];
  }

  // sample microfacet normal
  glm::vec2 const u = random.SampleUniform2();
  glm::vec3 const wo =
    ReorientHemisphere(
      glm::normalize(Cartesian(glm::sqrt(u.y), glm::Tau*u.x)), surface.normal
    );
  float pdf = glm::max(0.0f, glm::InvPi*glm::dot(wo, surface.normal));

  // perfect specular reflection based on microfacet normal TODO

  // evaluate MIS pdf using balance heuristic
  pdf = 0.0f;
  for (size_t i = 0; i < moment.alphas.size(); ++ i) {
    float const microfacetPdf = 1.0f/(4.0f * glm::dot(wi, surface.normal));
    pdf += (weights[i]/cumulativeW) * microfacetPdf;
  }

  // evaluate BRDF and weight
  glm::vec3 bsdfFs =
    Evaluate(
      structure, surface
    , wo
    , make_span(alphas), make_span(etas), make_span(kappas)
    );
  assert(!glm::isnan(bsdfFs));

  return {
    wo, bsdfFs, pdf
  };
}
