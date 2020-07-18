/*
*/

#include "fileutil.hpp"
#include "ui.hpp"

#include <monte-toad/log.hpp>
#include <monte-toad/renderinfo.hpp>
#include <monte-toad/material/layered.hpp>
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
  self.outputFile            = result["output"]          .as<std::string>();
  self.viewImageOnCompletion = result["view"]            .as<bool>();
  self.displayProgress       = !result["noprogress"]     .as<bool>();
  self.numThreads            = result["num-threads"]     .as<uint16_t>();

  /* { // camera origin */
  /*   auto value = result["camera-origin"].as<std::vector<float>>(); */
  /*   if (value.size() != 3) { */
  /*     spdlog::error("Camera origin must be in format X,Y,Z"); */
  /*     value = {{ 1.0f, 0.0f, 0.0f }}; */
  /*   } */
  /*   self.cameraOrigin = glm::vec3(value[0], value[1], value[2]); */
  /* } */

  /* { // camera up axis */
  /*   auto value = result["up-axis"].as<bool>(); */
  /*   // only change from default if flag is set */
  /*   if (value) { self.cameraUpAxis = glm::vec3(0.0f, 0.0f, -1.0f); } */
  /* } */

  /* { // camera direction (from target) */
  /*   auto value = result["camera-target"].as<std::vector<float>>(); */
  /*   if (value.size() != 3) { */
  /*     spdlog::error("Camera target must be in format X,Y,Z"); */
  /*     value = {{ 0.0f, 0.0f, 0.0f }}; */
  /*   } */
  /*   self.cameraDirection = */
  /*     glm::vec3(value[0], value[1], value[2]) - self.cameraOrigin; */
  /* } */

  { // debug print
    auto value = result["debug"].as<bool>();
    if (value) { spdlog::set_level(spdlog::level::debug); }
  }

  if (self.numThreads == 0) { self.numThreads = omp_get_max_threads()-1; }

  return self;
}

} // -- namespace

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {

  // -- setup logger so everything can be caught
  ui::InitializeLogger();

  // -- setup options
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

  // -- load up renderinfo from command line
  mt::RenderInfo render;
  { // -- parse options
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      printf("%s\n", options.help().c_str());
      return 0;
    }

    render = ParseRenderInfo(result);
  }

  omp_set_num_threads(render.numThreads);

  // -- load up renderinfo & plugin from config file
  mt::PluginInfo plugin;
  fileutil::LoadEditorConfig(render, plugin);

    mt::material::layered::Data data;
    data.layers = { mt::material::layered::Data::Layer{0.0f, 0.2f, 0.2f, 0.9f} };

  ui::Initialize(render, plugin);

  ui::Run(render, plugin);

  printf("\n");

  return 0;
}
