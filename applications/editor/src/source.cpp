/*
*/

#include "ui.hpp"

#include <monte-toad/log.hpp>
#include <monte-toad/scene.hpp>
#include <mt-plugin-host/plugin.hpp>
#include <mt-plugin/plugin.hpp>

#include <cxxopts.hpp>
#include <glm/glm.hpp>
#include <omp.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <string>
#include <vector>

namespace {
////////////////////////////////////////////////////////////////////////////////
mt::RenderInfo ParseRenderInfo(cxxopts::ParseResult const & result) {
  mt::RenderInfo self;
  self.modelFile             = result["file"]            .as<std::string>();
  self.outputFile            = result["output"]          .as<std::string>();
  self.environmentMapFile    = result["environment-map"] .as<std::string>();
  self.viewImageOnCompletion = result["view"]            .as<bool>();
  self.cameraFieldOfView     = result["fov"]             .as<float>();
  self.displayProgress       = !result["noprogress"]     .as<bool>();
  self.numThreads            = result["num-threads"]     .as<uint16_t>();
  self.bvhUse                = !result["no-bvh"]         .as<bool>();
  self.bvhOptimize           = !result["no-optimize-bvh"].as<bool>();
  self.samplesPerPixel       = result["spp"]             .as<uint32_t>();
  self.pathsPerSample        = result["pps"]             .as<uint32_t>();

  { // resolution
    auto value = result["resolution"].as<std::vector<uint32_t>>();
    if (value.size() != 2) {
      spdlog::error("Resolution must be in format Width,Height");
      value = { 640, 480 };
    }
    self.imageResolution[0] = value[0];
    self.imageResolution[1] = value[1];
  }

  { // camera origin
    auto value = result["camera-origin"].as<std::vector<float>>();
    if (value.size() != 3) {
      spdlog::error("Camera origin must be in format X,Y,Z");
      value = {{ 1.0f, 0.0f, 0.0f }};
    }
    self.cameraOrigin = glm::vec3(value[0], value[1], value[2]);
  }

  { // camera up axis
    auto value = result["up-axis"].as<bool>();
    // only change from default if flag is set
    if (value) { self.cameraUpAxis = glm::vec3(0.0f, 0.0f, -1.0f); }
  }

  { // camera origin
    auto value = result["camera-target"].as<std::vector<float>>();
    if (value.size() != 3) {
      spdlog::error("Camera target must be in format X,Y,Z");
      value = {{ 0.0f, 0.0f, 0.0f }};
    }
    self.cameraTarget = glm::vec3(value[0], value[1], value[2]);
  }

  { // debug print
    auto value = result["debug"].as<bool>();
    if (value) { spdlog::set_level(spdlog::level::debug); }
  }

  if (self.numThreads == 0) { self.numThreads = omp_get_max_threads()-1; }

  return self;
}

} // -- end anon namespace

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {
  auto options = cxxopts::Options("monte-toad-editor", "live raytrace editor");
  options.add_options()
    (
      "f,file", "input model file (required)",
      cxxopts::value<std::string>()->default_value("")
    ) (
      "d,debug", "enable debug printing"
    , cxxopts::value<bool>()->default_value("false")
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
      "spp", "number of iterations/samples per pixel (spp)"
    , cxxopts::value<uint32_t>()->default_value("8")
    ) (
      "pps", "number of paths per sample"
    , cxxopts::value<uint32_t>()->default_value("4")
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

  mt::RenderInfo renderInfo;
  { // -- parse options
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      printf("%s\n", options.help().c_str());
      return 0;
    }

    renderInfo = ParseRenderInfo(result);
  }

  omp_set_num_threads(renderInfo.numThreads);

  ui::Initialize(renderInfo);

  mt::PluginInfo pluginInfo;

  ui::Run(renderInfo, pluginInfo);

  mt::FreePlugins();

  /* spdlog::info("Loading model {}", renderInfo.modelFile); */
  /* auto scene = */
  /*   Scene::Construct( // TODO just pass renderInfo */
  /*     renderInfo.modelFile */
  /*   , renderInfo.environmentMapFile */
  /*   , renderInfo.bvhOptimize */
  /* ); */
  /* if (scene.meshes.size() == 0) { */
  /*   spdlog::error("Model loading failed, exitting"); */
  /*   return 1; */
  /* } */

  /* spdlog::debug("Model triangles: {}", scene.accelStructure->triangles.size()); */
  /* spdlog::debug("Model meshes: {}", scene.meshes.size()); */
  /* spdlog::debug("Model textures: {}", scene.textures.size()); */
  /* spdlog::debug("Model bounds: {} -> {}", scene.bboxMin, scene.bboxMax); */
  /* spdlog::debug("Model center: {}", (scene.bboxMax + scene.bboxMin) * 0.5f); */
  /* spdlog::debug("Model size: {}", (scene.bboxMax - scene.bboxMin)); */
  /* spdlog::debug( */
  /*   "BVH nodes: {}" */
  /* , scene.accelStructure->boundingVolume.node_count */
  /* ); */

  /* Camera camera; */
  /* { // -- camera setup */
  /*   camera.up = glm::vec3(0.0f, 1.0f, 0.0f); */
  /*   camera.fov = cameraFov; */
  /*   camera.lookat = cameraLookat; */
  /*   camera.ori = cameraOrigin; */
  /* } */

  /* { // -- render scene */
  /*   auto buffer = Buffer::Construct(resolution.x, resolution.y); */
  /*   spdlog::info( */
  /*     "Rendering at resolution ({}, {})", */
  /*     buffer.Width(), buffer.Height() */
  /*   ); */

  /*   std::atomic<size_t> progress = 0u; */
  /*   int const masterTid = omp_get_thread_num(); */
  /*   size_t mainThreadUpdateIt = 0; */

  /*   std::vector<GenericNoiseGenerator> noiseGenerators; */
  /*   noiseGenerators.resize(numThreads); */

  /*   { // construct noise */
  /*     auto n = Noise<NoiseType::White>::Construct(); */
  /*     for (size_t i = 0; i < numThreads; ++ i) { */
  /*       noiseGenerators[i] = n; */
  /*     } */
  /*   } */

  /*   for (size_t j = 0u; j < 128u; ++ j) { */
  /*     auto const div = buffer.Width()*buffer.Height()/128u; */
  /*     #pragma omp parallel for */
  /*     for (size_t ij = 0u; ij < div; ++ ij) { */
  /*       size_t i = j*div + ij; */
  /*       if (i >= buffer.Width()*buffer.Height()) { continue; } */
  /*       // -- render out at pixel X, Y */
  /*       size_t x = i%buffer.Width(), y = i/buffer.Width(); */
  /*       glm::vec2 uv = glm::vec2(x, y) / buffer.Dim(); */
  /*       uv = (uv - 0.5f) * 2.0f; */
  /*       uv.y *= buffer.AspectRatio(); */

  /*       buffer.At(i) = */
  /*         Render( */
  /*           uv */
  /*         , glm::vec2(buffer.Width(), buffer.Height()) */
  /*         , noiseGenerators[omp_get_thread_num()] */
  /*         , scene */
  /*         , camera */
  /*         , samplesPerPixel */
  /*         , pathsPerSample */
  /*         , useBvh */
  /*         ); */

  /*       // record & emit progress */
  /*       progress += 1; */
  /*       if (displayProgress */
  /*        && omp_get_thread_num() == masterTid */
  /*        && (++mainThreadUpdateIt)%5 == 0 */
  /*       ) { */
  /*         PrintProgress( */
  /*           progress/static_cast<float>(buffer.Width()*buffer.Height()) */
  /*         ); */
  /*       } */
  /*     } */
  /*   } */

  /*   if (displayProgress) */
  /*   { */
  /*     PrintProgress(1.0f); */
  /*     printf("\n"); // new line for progress bar */
  /*   } */

  /*   SaveImage(buffer, outputFile + "-prev-hdr", displayProgress); */

  /*   { */
  /*     float */
  /*       hdrMinimum = std::numeric_limits<float>::max() */
  /*     , hdrAverage = 0.0f */
  /*     , hdrMaximum = std::numeric_limits<float>::min(); */

  /*     for (size_t i = 0; i < buffer.Height()*buffer.Width(); ++ i) { */
  /*       float const value = glm::length(buffer.At(i)); */
  /*       hdrMinimum = glm::min(value, hdrMinimum); */
  /*       hdrMaximum = glm::max(value, hdrMaximum); */
  /*       hdrAverage += value; */
  /*     } */
  /*     hdrAverage /= static_cast<float>(buffer.Height()*buffer.Width()); */


  /*     #pragma omp parallel for */
  /*     for (size_t i = 0; i < buffer.Height()*buffer.Width(); ++ i) { */
  /*       auto & value = buffer.At(i); */
  /*       value = */
  /*         (value - glm::mix(hdrMinimum, hdrAverage, 0.2f)) */
  /*       * glm::rcp( */
  /*           glm::mix(hdrMaximum - hdrMinimum, hdrAverage - hdrMinimum, 0.8f) */
  /*         ); */
  /*     } */
  /*   } */

  /*   SaveImage(buffer, outputFile, displayProgress); */
  /* } */

  /* #ifdef __unix__ */
  /*   if (view) { */
  /*     system( */
  /*       ( */
  /*         std::string{"feh --full-screen --force-aliasing "} + outputFile + "*" */
  /*       ).c_str() */
  /*     ); */
  /*   } */
  /* #endif */
  // TODO windows, mac, etc

  return 0;
}
