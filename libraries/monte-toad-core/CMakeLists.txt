add_library(monte-toad-core STATIC)

target_include_directories(monte-toad-core PUBLIC "include/")
target_sources(
  monte-toad-core
  PRIVATE
    src/core/accelerationstructure.cpp
    src/core/any.cpp
    src/core/camerainfo.cpp
    src/core/enum.cpp
    src/core/geometry.cpp
    src/core/glutil.cpp
    src/core/math.cpp
    src/core/renderinfo.cpp
    src/core/scene.cpp
    src/core/surfaceinfo.cpp
    src/core/texture.cpp
    src/core/triangle.cpp
)

# TODO add spdlog as proper dependency
target_compile_definitions(
  monte-toad-core
  PUBLIC
    SPDLOG_NO_EXCEPTIONS=1
    SPDLOG_FMT_EXTERNAL=1
)

set_target_properties(
  monte-toad-core
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(
  monte-toad-core
  PUBLIC
    glm spdlog fmt bvh monte-toad-util
  PRIVATE
    glad mt-plugin
)
