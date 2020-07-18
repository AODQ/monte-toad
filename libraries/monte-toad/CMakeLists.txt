add_library(monte-toad STATIC)

target_include_directories(monte-toad PUBLIC "include/")
target_sources(
  monte-toad
  PRIVATE
    src/enum.cpp
    src/geometry.cpp
    src/glutil.cpp
    src/imagebuffer.cpp
    src/imgui.cpp
    src/material/layered.cpp
    src/log.cpp
    src/math.cpp
    src/primitive.cpp
    src/renderinfo.cpp
    src/scene.cpp
    src/surfaceinfo.cpp
    src/texture.cpp
)

# TODO add spdlog as proper dependency...
target_compile_definitions(
  monte-toad
  PUBLIC
    SPDLOG_NO_EXCEPTIONS=1
    SPDLOG_FMT_EXTERNAL=1
    JSON_NOEXCEPTION=1
)

find_package(glm REQUIRED FATAL_ERROR)
find_package(assimp REQUIRED)
find_package(OpenMP)


set_target_properties(monte-toad PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(monte-toad PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(monte-toad PUBLIC
  glm glad spdlog fmt OpenMP::OpenMP_CXX bvh stb assimp mt-plugin imgui
)
