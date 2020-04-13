/*
*/

#include "io.hpp"
#include "log.hpp"
#include "noise.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "unittest.hpp"

#include <cxxopts.hpp>
#include <glm/glm.hpp>
#include <omp.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <string>
#include <vector>


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
      "O,camera-origin", "camera origin"
    , cxxopts::value<std::vector<float>>()->default_value("1.0f,1.0f,1.0f")
    ) (
      "T,camera-target", "camera lookat target"
    , cxxopts::value<std::vector<float>>()->default_value("0.0f,0.0f,0.0f")
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
      "j,num-threads", "number of worker threads, 0 is automatic"
    , cxxopts::value<uint16_t>()->default_value("0")
    ) (
      "unit-test", "run unit tests"
    , cxxopts::value<bool>()->default_value("false")
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
  bool         unittest;
  bool         view;
  glm::vec3 cameraOrigin;
  glm::vec3 cameraLookat;
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
    unittest        = result["unit-test"]      .as<bool>();
    view            = result["view"]           .as<bool>();
    cameraFov       = result["fov"]            .as<float>();
    displayProgress = result["noprogress"]     .as<bool>();
    upAxisZ         = result["up-axis"]        .as<bool>();
    numThreads      = result["num-threads"]    .as<uint16_t>();
    useBvh          = result["no-bvh"]         .as<bool>();
    optimizeBvh     = result["no-optimize-bvh"].as<bool>();
    samplesPerPixel = result["spp"]            .as<uint32_t>();
    environmentMapFilename = result["environment-map"].as<std::string>();

    { // resolution
      auto value = result["resolution"].as<std::vector<uint32_t>>();
      if (value.size() != 2) {
        spdlog::error("Resolution must be in format Width,Height");
        return 1;
      }
      resolution = glm::i32vec2(value[0], value[1]);
    }

    { // camera origin
      auto value = result["camera-origin"].as<std::vector<float>>();
      if (value.size() != 3) {
        spdlog::error("Camera origin must be in format X,Y,Z");
        return 1;
      }
      cameraOrigin = glm::vec3(value[0], value[1], value[2]);
    }

    { // camera origin
      auto value = result["camera-target"].as<std::vector<float>>();
      if (value.size() != 3) {
        spdlog::error("Camera target must be in format X,Y,Z");
        return 1;
      }
      cameraLookat = glm::vec3(value[0], value[1], value[2]);
    }

    // convert command-line units to application units
    cameraFov       = 180.0f - cameraFov;
    displayProgress = !displayProgress;
    useBvh          = !useBvh;
    optimizeBvh     = !optimizeBvh;
  }

  numThreads = numThreads == 0 ? omp_get_max_threads()-1 : numThreads;
  omp_set_num_threads(numThreads);

  spdlog::info("Loading model {}", inputFile);
  auto scene =
    Scene::Construct(inputFile, environmentMapFilename, upAxisZ, optimizeBvh);
  if (scene.meshes.size() == 0) {
    spdlog::error("Model loading failed, exitting");
    return 1;
  }

  spdlog::info("Model triangles: {}", scene.accelStructure->triangles.size());
  spdlog::info("Model meshes: {}", scene.meshes.size());
  spdlog::info("Model textures: {}", scene.textures.size());
  spdlog::info("Model bounds: {} -> {}", scene.bboxMin, scene.bboxMax);
  spdlog::info("Model center: {}", (scene.bboxMax + scene.bboxMin) * 0.5f);
  spdlog::info("Model size: {}", (scene.bboxMax - scene.bboxMin));
  spdlog::info(
    "BVH nodes: {}"
  , scene.accelStructure->boundingVolume.node_count
  );
  spdlog::info(
    "BVH traversal cost: {}"
  , scene.accelStructure->boundingVolume.traversal_cost
  );

  Camera camera;
  { // -- camera setup
    camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
    camera.fov = cameraFov;
    camera.lookat = cameraLookat;
    camera.ori = cameraOrigin;
  }

  if (unittest) {
    Test();
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

    std::vector<GenericNoiseGenerator> noiseGenerators;
    noiseGenerators.resize(numThreads);

    { // construct noise
      auto n = Noise<NoiseType::White>::Construct();
      for (size_t i = 0; i < numThreads; ++ i) {
        noiseGenerators[i] = n;
      }
    }

    #pragma omp parallel for
    for (size_t i = 0u; i < buffer.Width()*buffer.Height(); ++ i) {
      // -- render out at pixel X, Y
      size_t x = i%buffer.Width(), y = i/buffer.Width();
      glm::vec2 uv = glm::vec2(x, y) / buffer.Dim();
      uv = (uv - 0.5f) * 2.0f;
      uv.y *= buffer.AspectRatio();

      buffer.At(i) =
        Render(
          uv
        , glm::vec2(buffer.Width(), buffer.Height())
        , noiseGenerators[omp_get_thread_num()]
        , scene
        , camera
        , samplesPerPixel
        , useBvh
        );

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

    SaveImage(buffer, outputFile, displayProgress);
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
