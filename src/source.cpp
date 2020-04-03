#include "scene.hpp"
#include "ext/cxxopts.hpp"

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
    return this->width/static_cast<float>(this->height);
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
    if (i <  40.0f*progress) printf("=");
    if (i == 40.0f*progress) printf(">");
    if (i >  40.0f*progress) printf(" ");
  }
  // leading spaces in case of terminal/text corruption
  printf("] %0.1f%%   \r", progress*100.0f);
  fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
void SaveImage(std::string const & filename, Buffer const & data) {
  auto file = std::ofstream{filename, std::ios::binary};
  spdlog::info("Saving image {}", filename);
  file << "P6\n" << data.Width() << " " << data.Height() << "\n255\n";
  for (size_t i = 0u; i < data.Height()*data.Width(); ++ i) {
    glm::vec3 pixel = data.At(i);
    pixel = 255.0f * glm::clamp(pixel, glm::vec3(0.0f), glm::vec3(1.0f));
    file
      << static_cast<uint8_t>(pixel.x)
      << static_cast<uint8_t>(pixel.y)
      << static_cast<uint8_t>(pixel.z)
    ;

    if (i%data.Width() == 0)
      PrintProgress(i/static_cast<float>(data.Height()*data.Width()));
  }
  PrintProgress(1.0f);
  printf("\n"); // new line for progress bar
  spdlog::info("Finished saving image {}", filename);
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 LookAt(glm::vec3 dir, glm::vec2 uv) {
  glm::vec3
    ww = glm::normalize(dir),
    uu = glm::normalize(glm::cross(ww, glm::vec3(0.0, 1.0, 0.0))),
    vv = glm::normalize(glm::cross(uu, ww));
  return
    glm::normalize(
      glm::mat3(uu, vv, ww)
    * glm::vec3(uv.x, uv.y, glm::radians(120.0f))
    );
}

////////////////////////////////////////////////////////////////////////////////
struct Camera {
  glm::vec3 ori;
  glm::vec3 lookat;
};

////////////////////////////////////////////////////////////////////////////////
glm::vec3 BarycentricInterpolation(
  glm::vec3 const & v0, glm::vec3 const & v1, glm::vec3 const & v2
, glm::vec2 const & uv
) {
  return uv.x * v0 + uv.y * v1 + (1.0f - uv.x - uv.y) * v2;
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 Render(
  glm::vec2 const uv
, Scene const & scene
, Camera const & camera
) {
  glm::vec3 eyeOri, eyeDir;
  eyeOri = camera.ori;
  eyeDir = LookAt(glm::normalize(camera.lookat - camera.ori), uv);

  float triDistance; glm::vec2 triUv;
  Triangle const * triangle = Raycast(scene, eyeOri, eyeDir, triDistance, triUv);

  if (!triangle) { return glm::vec3(0.0f, 0.0f, 0.0f); }

  glm::vec3 normal =
    BarycentricInterpolation(triangle->n0, triangle->n1, triangle->n2, triUv);

  return glm::vec3(1.0f) * glm::dot(normal, glm::vec3(-0.5f, 0.5f, 0.5f));
}

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
      "f,file", "Input model file (required)", cxxopts::value<std::string>()
    ) (
      "o,output", "Image output file"
    , cxxopts::value<std::string>()->default_value("out.ppm")
    ) (
      "v,view", "View image on completion"
    , cxxopts::value<bool>()->default_value("false")
    ) (
      "T,camera-theta", "Camera origin around origin as theta angle"
    , cxxopts::value<float>()->default_value("0.0f")
    ) (
      "H,camera-height", "Camera (scaled) height"
    , cxxopts::value<float>()->default_value("0.5f")
    ) (
      "D,camera-distance", "Camera (scaled) distance"
    , cxxopts::value<float>()->default_value("1.0f")
    ) (
      "h,help", "Print usage"
    )
  ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    spdlog::info("{}", options.help());
    return 0;
  }

  std::string inputFile;
  std::string outputFile;
  bool        view;
  float       cameraTheta;
  float       cameraDist;
  float       cameraHeight;

  { // -- collect values
    if (!result["file"].count()) {
      spdlog::error("Must provide an input file");
      return 1;
    }
    inputFile    = result["file"]           .as<std::string>();
    outputFile   = result["output"]         .as<std::string>();
    view         = result["view"]           .as<bool>();
    cameraTheta  = result["camera-theta"]   .as<float>();
    cameraDist   = result["camera-distance"].as<float>();
    cameraHeight = result["camera-height"]  .as<float>();
  }

  spdlog::info("Loading model {}", inputFile);
  auto scene = Scene::Construct(inputFile);
  spdlog::info("Model triangles: {}", scene.scene.size());
  spdlog::info("Model bounds: {} -> {}", scene.bboxMin, scene.bboxMax);
  spdlog::info("Model center: {}", (scene.bboxMax + scene.bboxMin) * 0.5f);
  spdlog::info("Model size: {}", (scene.bboxMax - scene.bboxMin));

  Camera camera;
  { // -- camera setp
    camera.lookat = (scene.bboxMax + scene.bboxMin) * 0.5f;
    camera.ori =
      camera.lookat
    + (scene.bboxMax - scene.bboxMin)
    * glm::vec3(glm::cos(cameraTheta), cameraHeight, glm::sin(cameraTheta))
    * cameraDist
    ;
  }

  { // -- render scene
    auto buffer = Buffer::Construct(640, 480);
    spdlog::info(
      "Rendering at resolution ({}, {})",
      buffer.Width(), buffer.Height()
    );

    std::atomic<size_t> progress = 0u;
    int const masterTid = omp_get_thread_num();
    #pragma omp parallel for
    for (size_t i = 0u; i < buffer.Width()*buffer.Height(); ++ i) {
      // -- render out at pixel X, Y
      size_t x = i%buffer.Width(), y = i/buffer.Width();
      glm::vec2 uv = glm::vec2(x, y * buffer.AspectRatio()) / buffer.Dim();
      uv = (uv - 0.5f) * 2.0f;
      buffer.At(i) = Render(uv, scene , camera);

      // record & emit progress
      progress += 1;
      if (omp_get_thread_num() == masterTid) {
        PrintProgress(
          progress/static_cast<float>(buffer.Width()*buffer.Height())
        );
      }
    }
    PrintProgress(1.0f);
    printf("\n"); // new line for progress bar

    SaveImage(outputFile, buffer);
  }

  #ifdef __unix__
    if (view) { system((std::string{"feh "} + outputFile).c_str()); }
  #endif

  return 0;
}
