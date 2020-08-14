add_library(monte-toad-core STATIC)

target_include_directories(monte-toad-core PUBLIC "include/")
target_sources(
  monte-toad-core
  PRIVATE
    src/core/any.cpp
    src/core/camerainfo.cpp
    src/core/enum.cpp
    src/core/geometry.cpp
    src/core/glutil.cpp
    src/core/integratordata.cpp
    src/core/math.cpp
    src/core/renderinfo.cpp
    src/core/scene.cpp
    src/core/surfaceinfo.cpp
    src/core/texture.cpp
    src/core/triangle.cpp
)

set_target_properties(
  monte-toad-core
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(
  monte-toad-core
  PUBLIC
    glm spdlog::spdlog monte-toad-util
  PRIVATE
    glad mt-plugin imgui bvh omp
)
