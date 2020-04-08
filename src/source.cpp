/*
*/

#include "render.hpp"
#include "scene.hpp"

#include "../ext/cxxopts.hpp"
#include <glm/glm.hpp>
#include <omp.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <fstream>
#include <string>
#include <vector>

////////////////////////////////////////////////////////////////////////////////
struct Buffer {
  Buffer() = default;
private:
  std::vector<glm::vec3> data;
  size_t width, height;

public:
  static Buffer Construct(size_t width, size_t height) {
    Buffer self;
    self.width = width; self.height = height;
    self.data.resize(width*height);
    return self;
  }

  float AspectRatio() const {
    return this->height/static_cast<float>(this->width);
  }

  size_t Width() const { return this->width; }
  size_t Height() const { return this->height; }
  glm::vec2 Dim() const { return glm::vec2(this->width, this->height); }

  glm::vec3 & At(size_t x, size_t y) {
    return this->data[y*this->width + x];
  }

  glm::vec3 const & At(size_t x, size_t y) const {
    return this->data[y*this->width + x];
  }

  glm::vec3 & At(size_t it) { return this->At(it%this->width, it/this->width); }
  glm::vec3 const & At(size_t it) const {
    return this->At(it%this->width, it/this->width);
  }
};

////////////////////////////////////////////////////////////////////////////////
void PrintProgress(float progress) {
  printf("[");
  for (int i = 0; i < 40; ++ i) {
    if (i <  static_cast<int>(40.0f*progress)) printf("=");
    if (i == static_cast<int>(40.0f*progress)) printf(">");
    if (i >  static_cast<int>(40.0f*progress)) printf(" ");
  }
  // leading spaces in case of terminal/text corruption
  printf("] %0.1f%%   \r", progress*100.0f);
  fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
void SaveImage(
  std::string const & filename
, Buffer const & data
, bool displayProgress
) {
  auto file = std::ofstream{filename, std::ios::binary};
  spdlog::info("Saving image {}", filename);
  file << "P6\n" << data.Width() << " " << data.Height() << "\n255\n";
  for (size_t i = 0u; i < data.Height()*data.Width(); ++ i) {
    // flip Y
    glm::vec3 pixel = data.At(i%data.Width(), data.Height()-i/data.Width()-1);
    pixel = 255.0f * glm::clamp(pixel, glm::vec3(0.0f), glm::vec3(1.0f));
    file
      << static_cast<uint8_t>(pixel.x)
      << static_cast<uint8_t>(pixel.y)
      << static_cast<uint8_t>(pixel.z)
    ;

    if (displayProgress && i%data.Width() == 0)
      PrintProgress(i/static_cast<float>(data.Height()*data.Width()));
  }

  if (displayProgress) {
    PrintProgress(1.0f);
    printf("\n"); // new line for progress bar
  }

  spdlog::info("Finished saving image {}", filename);
}

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
int main(int argc, char** argv) {
  auto options = cxxopts::Options("cpuraytracer", "Raytraces models to image");
  options.add_options()
    (
      "f,file", "input model file (required)", cxxopts::value<std::string>()
    ) (
      "o,output", "image output file"
    , cxxopts::value<std::string>()->default_value("out.ppm")
    ) (
      "v,view", "view image on completion"
    , cxxopts::value<bool>()->default_value("false")
    ) (
      "T,camera-theta", "camera origin as theta angle (degrees)"
    , cxxopts::value<float>()->default_value("0.0f")
    ) (
      "H,camera-height", "camera (scaled) height"
    , cxxopts::value<float>()->default_value("0.5f")
    ) (
      "r,resolution", "window resolution \"Width,Height\"",
      cxxopts::value<std::vector<uint32_t>>()->default_value("640,480")
    ) (
      "e,environment-map"
    , "environment map texture location (must be in spherical format, for now."
    , cxxopts::value<std::string>()->default_value("")
    ) (
      "no-bvh", "disallows use of BVH acceleration structure"
    , cxxopts::value<bool>()->default_value("false")
    ) (
      "no-optimize-bvh"
    , "disables bvh tree optimization (slower construction, faster traversal"
    , cxxopts::value<bool>()->default_value("false")
    ) (
      "D,camera-distance", "camera (scaled) distance"
    , cxxopts::value<float>()->default_value("1.0f")
    ) (
      "j,num-threads", "number of worker threads, 0 is automatic"
    , cxxopts::value<uint16_t>()->default_value("0")
    ) (
      "spp", "number of iterations/samples per pixel (spp)"
    , cxxopts::value<uint32_t>()->default_value("8")
    ) (
      "U,up-axis", "model up-axis set to Z (Y when not set)"
    , cxxopts::value<bool>()->default_value("false")
    ) (
      "F,fov", "camera field-of-view (degrees)"
    , cxxopts::value<float>()->default_value("90.0f")
    ) (
      "p,noprogress", "does not display progress"
    , cxxopts::value<bool>()->default_value("false")
    ) (
      "h,help", "print usage"
    )
  ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    printf("%s\n", options.help().c_str());
    return 0;
  }

  std::string  inputFile;
  std::string  outputFile;
  bool         view;
  float        cameraTheta;
  float        cameraDist;
  float        cameraHeight;
  float        cameraFov;
  glm::i32vec2 resolution;
  std::string  environmentMapFilename;
  bool         upAxisZ;
  bool         displayProgress;
  bool         useBvh;
  bool         optimizeBvh;
  uint16_t     numThreads;
  uint32_t     samplesPerPixel;

  { // -- collect values
    if (!result["file"].count()) {
      spdlog::error("Must provide an input file");
      return 1;
    }
    inputFile       = result["file"]           .as<std::string>();
    outputFile      = result["output"]         .as<std::string>();
    view            = result["view"]           .as<bool>();
    cameraTheta     = result["camera-theta"]   .as<float>();
    cameraDist      = result["camera-distance"].as<float>();
    cameraHeight    = result["camera-height"]  .as<float>();
    cameraFov       = result["fov"]            .as<float>();
    displayProgress = result["noprogress"]     .as<bool>();
    upAxisZ         = result["up-axis"]        .as<bool>();
    numThreads      = result["num-threads"]    .as<uint16_t>();
    useBvh          = result["no-bvh"]         .as<bool>();
    optimizeBvh     = result["no-optimize-bvh"].as<bool>();
    samplesPerPixel = result["spp"]            .as<uint32_t>();
    environmentMapFilename = result["environment-map"].as<std::string>();

    { // resolution
      auto resolutionV = result["resolution"].as<std::vector<uint32_t>>();
      if (resolutionV.size() != 2) {
        spdlog::error("Resolution must be in format Width,Height");
        return 1;
      }
      resolution = glm::i32vec2(resolutionV[0], resolutionV[1]);
    }

    // convert command-line units to application units
    cameraTheta     = glm::radians(cameraTheta);
    cameraFov       = 180.0f - cameraFov;
    displayProgress = !displayProgress;
    useBvh          = !useBvh;
    optimizeBvh     = !optimizeBvh;
  }

  omp_set_num_threads(numThreads == 0 ? omp_get_max_threads()-1 : numThreads);

  spdlog::info("Loading model {}", inputFile);
  auto scene =
    Scene::Construct(inputFile, environmentMapFilename, upAxisZ, optimizeBvh);
  if (scene.meshes.size() == 0) {
    spdlog::error("Model loading failed, exitting");
    return 1;
  }

  spdlog::info("Model triangles: {}", scene.accelStructure.triangles.size());
  spdlog::info("Model meshes: {}", scene.meshes.size());
  spdlog::info("Model textures: {}", scene.textures.size());
  spdlog::info("Model bounds: {} -> {}", scene.bboxMin, scene.bboxMax);
  spdlog::info("Model center: {}", (scene.bboxMax + scene.bboxMin) * 0.5f);
  spdlog::info("Model size: {}", (scene.bboxMax - scene.bboxMin));
  spdlog::info(
    "BVH nodes: {}"
  , scene.accelStructure.boundingVolume.node_count
  );
  spdlog::info(
    "BVH traversal cost: {}"
  , scene.accelStructure.boundingVolume.traversal_cost
  );

  Camera camera;
  { // -- camera setup
    camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
    camera.fov = cameraFov;
    camera.lookat = (scene.bboxMax + scene.bboxMin) * 0.5f;
    camera.ori =
      (scene.bboxMax + scene.bboxMin) * 0.5f
    + (scene.bboxMax - scene.bboxMin)
    * glm::vec3(glm::cos(cameraTheta), cameraHeight, glm::sin(cameraTheta))
    * cameraDist
    ;
  }

  { // -- render scene
    auto buffer = Buffer::Construct(resolution.x, resolution.y);
    spdlog::info(
      "Rendering at resolution ({}, {})",
      buffer.Width(), buffer.Height()
    );

    std::atomic<size_t> progress = 0u;
    int const masterTid = omp_get_thread_num();
    size_t mainThreadUpdateIt = 0;

    #pragma omp parallel for
    for (size_t i = 0u; i < buffer.Width()*buffer.Height(); ++ i) {
      // -- render out at pixel X, Y
      size_t x = i%buffer.Width(), y = i/buffer.Width();
      glm::vec2 uv = glm::vec2(x, y) / buffer.Dim();
      uv = (uv - 0.5f) * 2.0f;
      uv.y *= buffer.AspectRatio();

      glm::vec3 accumulatedColor = glm::vec3(0.0f);
      size_t collectedIt = 0;

      for (uint32_t it = 0; it < samplesPerPixel; ++ it) {
        auto renderResults = Render(uv, scene, camera, useBvh);
        if (!renderResults.valid) { continue; }
        // apply averaging to accumulated color corresponding to current
        // collected it
        accumulatedColor =
          glm::mix(
            accumulatedColor
          , renderResults.color
          , collectedIt/static_cast<float>(collectedIt+1)
          );
        ++ collectedIt;
      }

      buffer.At(i) = accumulatedColor;

      // record & emit progress
      progress += 1;
      if (displayProgress
       && omp_get_thread_num() == masterTid
       && (++mainThreadUpdateIt)%5 == 0
      ) {
        PrintProgress(
          progress/static_cast<float>(buffer.Width()*buffer.Height())
        );
      }
    }

    if (displayProgress)
    {
      PrintProgress(1.0f);
      printf("\n"); // new line for progress bar
    }

    SaveImage(outputFile, buffer, displayProgress);
  }

  #ifdef __unix__
    if (view) {
      system(
        (std::string{"feh --full-screen --force-aliasing "}
      + outputFile).c_str()
      );
    }
  #endif
  // TODO windows, mac, etc

  return 0;
}
