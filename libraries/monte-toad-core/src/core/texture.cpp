#include <monte-toad/core/texture.hpp>

#include <spdlog/spdlog.h>

/*

  Textures represent a set of discrete infinitesimal colour values (texels),
    and a filter to reconstruct the original image from these texels;

    ⌠1 ⌠1                      1 1
    │  │  p(img, u, v) du dv ≈ Σ Σ f(p, img[ΔuΔv], u, v)
    ⌡0 ⌡0                      u v

  where,
    u and v are the UV coordinates of the image, a floating-point [0 ‥ 1]
    Δu and Δv are the amount by which u/v are stepped per iteration
    img is the image, img[X] is the mipmap of the image
    p is a function that returns a texel value at coordinates;
    f is a filter-reconstruction function (nearest-neighbour, linear, etc)

  Ideally you would integrate along the entire image and use continous function
    p to collect texel values, which would be the ground-truth of the captured
    image. Because memory and processing is finite, these pixels can be
    approximated using p as a discrete function to grab individual texels, and
    a filter reconstructing function f to approximate the ground-truth by
    combining the texels in different manners.

  The filter is necessary since pixels (infinitesimal points on the rendered
    image) do not necessarily map directly to texels. The values of Δu and Δv
    can vary greatly, and uv can even extend past 1 in some cases (such as
    wrapping).

  The cheapest filter is the nearest-neighbor filter, which chooses the closest
    texel to represent the pixel. The mapping is simple;

    f(p, img, u, v) = p(img, round(u*width), round(v*height))

      where p in this case would take integer coordinates [0 ‥ width/height]

  Another filter is to mix the four closest pixels, 'bilinear' filter;

    f(p, img, u, v) =

       ⎛                   ⎞
      p⎜img, uₜ    , vₜ    ⎟ * fract(u*w) * fract(1 - v*h)
       ⎝                   ⎠

       ⎛                  1⎞
    + p⎜img, uₜ    , vₜ + ─⎟ * fract(u*w) * fract(v*h)
       ⎝                  h⎠

       ⎛          1        ⎞
    + p⎜img, uₜ + ─, vₜ    ⎟ * fract(1 - u*w) * fract(1 - v*h)
       ⎝          w        ⎠

       ⎛          1       1⎞
    + p⎜img, uₜ + ─, vₜ + ─⎟ * fract(1 - u*w) * fract(v*h)
       ⎝          w       h⎠

    where,
      uₜ, vₜ are the centers of the uv coordinate;

                                      1
        uₜ = u + (0.5 - fract(u*w)) * ─
                                      w

                                      1
        vₜ = v + (0.5 - fract(v*h)) * ─
                                      h

  In practice these textures are mapped to 3D surfaces. In those conditions, not
    even all available-texels will readily be useable;

    ┌────────────┐             │  ┌────────────┐
    │     ╱╲     │             │  │     ╱╲     │
    │    ╱**╲    │             │  │    ╱**╲    │
    │   ╱****╲   │             │  │   ╱****╲   │
    │  ╱******╲  │             │  │            │
    │            │             │  │            │
    └────────────┘             │  └────────────┘
    13 pixels are available to │  6 pixels are avilable to
    map to texels              │  map to texels

  Aliasing occurs when there are not enough pixels on the geometry to sample
    all texels. Precomputing mipmaps by halving an image's dimension N times
    (to produce N mipmaps) means that (W*H)/2^N pixels need to be sampled to
    produce a good approximation of the image. Thus, another way to put this,
    when a geometry's screen-space area decreases, the less pixels can map to
    texels, and the higher mipmap-level is necessary to build a good
    approximation of the ground-truth image.
*/

////////////////////////////////////////////////////////////////////////////////
mt::core::Texture mt::core::Texture::Construct(
  size_t width, size_t height, size_t colorChannels, void const * data
) {
  mt::core::Texture texture;
  texture.width = width;
  texture.height = height;

  if (!data) {
    spdlog::critical("texture data nullptr");
    return texture;
  }

  texture.data.resize(texture.width*texture.height);
  for (size_t i = 0; i < texture.data.size(); ++ i) {
    size_t x = i%texture.width, y = i/texture.width;

    uint8_t const * byte =
      reinterpret_cast<uint8_t const *>(data)
    + texture.Idx(x, y)*sizeof(uint8_t)*4
    ;

    // load all color channels, in case of R, RGB, etc set the unused channels
    //   to 1.0f
    for (size_t j = 0; j < 4; ++ j)
      { texture.data[i][j] = colorChannels >= j ? *(byte + j)/255.0f : 1.0f; }
  }

  return texture;
}

////////////////////////////////////////////////////////////////////////////////
glm::vec4 mt::core::Sample(
  mt::core::Texture const & texture
, glm::vec2 uvCoords
) {
  uvCoords = glm::mod(uvCoords, glm::vec2(1.0f));
  /* uvCoords = glm::clamp(uvCoords, glm::vec2(0.0f), glm::vec2(1.0f)); */
  uvCoords.y = 1.0f - uvCoords.y;
  uint64_t
    x =
      glm::clamp(
        static_cast<uint64_t>(uvCoords.x*texture.width), 0ul, texture.width-1
      )
  , y =
      glm::clamp(
        static_cast<uint64_t>(uvCoords.y*texture.height), 0ul, texture.height-1
      )
  ;

  return texture.data[texture.Idx(x, y)];
}

////////////////////////////////////////////////////////////////////////////////
glm::vec4 mt::core::SampleBilinear(
  mt::core::Texture const & texture
, glm::vec2 uvCoords
) {
  glm::vec2 res = glm::vec2(texture.width, texture.height);

  glm::vec2 st = uvCoords*res - glm::vec2(0.5f);

  glm::vec2 iuv = glm::floor(st);
  glm::vec2 fuv = glm::fract(st);

  glm::vec4
    a = mt::core::Sample(texture, (iuv + glm::vec2(0.5f,0.5f))/res)
  , b = mt::core::Sample(texture, (iuv + glm::vec2(1.5f,0.5f))/res)
  , c = mt::core::Sample(texture, (iuv + glm::vec2(0.5f,1.5f))/res)
  , d = mt::core::Sample(texture, (iuv + glm::vec2(1.5f,1.5f))/res)
  ;

  return
    mix(
      mix(a, b, fuv.x)
    , mix( c, d, fuv.x)
    , fuv.y
    );
}

////////////////////////////////////////////////////////////////////////////////
//glm::vec4 mt::Sample(
//  mt::CubemapTexture const & /*texture*/, glm::vec3 /*dir*/
//) {
//  // TODO
//  return glm::vec4(1.0f);
//}

////////////////////////////////////////////////////////////////////////////////
glm::vec4 mt::core::Sample(mt::core::Texture const & texture, glm::vec3 dir) {
  return
    mt::core::Sample(
      texture,
      glm::vec2(
          0.5f + (glm::atan(dir.x, dir.z) / 6.283185307f)
        , 0.5f - (glm::asin(-dir.y)/3.14159f)
      )
    );
}

#include <monte-toad/core/scene.hpp>

#include <imgui/imgui.hpp>

bool SelectTexture(
  mt::core::Scene const & scene
, mt::core::Texture const * & tex
, std::string const & label
) {
  bool change = false;
  std::string const comboLabel = "Texture ##" + label;
  if (!ImGui::BeginCombo(comboLabel.c_str(), tex ? tex->label.c_str() : "none"))
    { return false; }

  if (ImGui::Selectable("none", !tex)) {
    tex = nullptr;
    change = true;
  }

  for (auto & sceneTex : scene.textures) {
    if (ImGui::Selectable(sceneTex.label.c_str(), &sceneTex == tex)) {
      tex = &sceneTex;
      change = true;
    }
  }

  ImGui::EndCombo();

  return change;
}

template <> bool mt::core::TextureOption<float>::GuiApply(
  mt::core::Scene const & scene
) {
  bool change = false;
  ImGui::Text("%s", this->label.c_str());
  if (!this->userTexture) {
    std::string const sliderLabel = "##" + this->label;
    change =
      ImGui::SliderFloat(
        sliderLabel.c_str(), &this->userValue, this->minRange, this->maxRange
      );
  }

  change |= SelectTexture(scene, this->userTexture, this->label);

  return change;
}

template <> float mt::core::TextureOption<float>::Get(
  glm::vec2 const & uv
) const {
  auto value = this->userValue;
  if (this->userTexture) {
    value = mt::core::Sample(*this->userTexture, uv).r;
    value = glm::mix(this->minRange, this->maxRange, value);
  }
  return value;
}

template <> bool mt::core::TextureOption<glm::vec3>::GuiApply(
  mt::core::Scene const & scene
) {
  bool change = false;
  ImGui::Text("%s", this->label.c_str());
  if (!this->userTexture) {
    std::string const sliderLabel = "##" + this->label;

    if (this->minRange == 0.0f && this->maxRange == 1.0f){
      change = ImGui::ColorPicker3(sliderLabel.c_str(), &this->userValue.x);
    } else {
      change =
        ImGui::SliderFloat3(
          sliderLabel.c_str()
        , &this->userValue.x, this->minRange, this->maxRange
        );
    }
  }

  change |= SelectTexture(scene, this->userTexture, this->label);

  return change;
}

template <> glm::vec3 mt::core::TextureOption<glm::vec3>::Get(
  glm::vec2 const & uv
) const {
  auto value = this->userValue;
  if (this->userTexture) {
    auto v = mt::core::Sample(*this->userTexture, uv);
    value.x = v.x; value.y = v.y; value.z = v.z;
    value =
      glm::mix(glm::vec3(this->minRange), glm::vec3(this->maxRange), value);
  }
  return value;
}
