add_library(monte-toad STATIC)

target_include_directories(monte-toad PUBLIC "include/")
target_sources(
  monte-toad
  PRIVATE
    src/engine.cpp
    src/imagebuffer.cpp
    src/log.cpp
    src/math.cpp
    src/primitive.cpp
    src/scene.cpp
    src/texture.cpp
)

# TODO add spdlog as proper dependency...
target_compile_definitions(
  monte-toad
  PUBLIC
    SPDLOG_NO_EXCEPTIONS=1
    SPDLOG_FMT_EXTERNAL=1
)

find_package(glm REQUIRED FATAL_ERROR)
find_package(assimp REQUIRED FATAL_ERROR)
find_package(OpenMP)

set_target_properties(monte-toad PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(monte-toad PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(monte-toad PUBLIC
  glm spdlog fmt OpenMP::OpenMP_CXX bvh stb assimp mt-plugin
)
