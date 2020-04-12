cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
cmake_policy(VERSION 3.5)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK    ccache)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message("[debug enabled]")
else()
  add_compile_definitions(NDEBUG)
endif()

project (
  cpuraytracer
  VERSION 0.0
  LANGUAGES CXX
  DESCRIPTION "cpu raytracer"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-Wall -fno-exceptions")

add_compile_definitions(
  SPDLOG_NO_EXCEPTIONS
  SPDLOG_FMT_EXTERNAL
  CXXOPTS_NO_EXCEPTIONS
)

## requires out of source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if (EXISTS "${LOC_PATH}")
  message(FATAL_ERROR "build must be out of source")
endif()

## setup project
set(NAME cpuraytracer)
project(${NAME} CXX)

## Add dependents
find_package(glm REQUIRED FATAL_ERROR)
find_package(assimp REQUIRED FATAL_ERROR)
find_package(OpenMP)

# Adds BVH dependency
set(BVH_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/bvh/include/
  CACHE PATH "bvh include directory"
)
add_library(bvh INTERFACE)
target_include_directories(bvh INTERFACE $<BUILD_INTERFACE:${BVH_INCLUDE_DIR}>)

# Adds cxxopts dependency
set(CXXOPTS_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/cxxopts/include/
  CACHE PATH "cxxopts include directory"
)
add_library(cxxopts INTERFACE)
target_include_directories(cxxopts
  INTERFACE $<BUILD_INTERFACE:${CXXOPTS_INCLUDE_DIR}>
)

## source list for application (src/include)
set(SOURCE_LIST
  "src/io.cpp"
  "src/noise.cpp"
  "src/primitive.cpp"
  "src/render.cpp"
  "src/scene.cpp"
  "src/source.cpp"
  "src/texture.cpp"
  "src/unittest.cpp"
)
set(HEADER_LIST
  "ext/stb_image.hpp"
  "src/io.hpp"
  "src/log.hpp"
  "src/noise.hpp"
  "src/primitive.hpp"
  "src/render.hpp"
  "src/scene.hpp"
  "src/span.hpp"
  "src/texture.hpp"
  "src/unittest.hpp"
)

## add exceutable w/ source list and set compile options
add_executable(cpuraytracer ${SOURCE_LIST})
target_compile_features(cpuraytracer PRIVATE cxx_std_20)

## link dependents
target_link_libraries(cpuraytracer
  PRIVATE
    glm spdlog fmt OpenMP::OpenMP_CXX bvh assimp foo cxxopts
)

## add include/source directories , sources support necessary for (lamer) IDE
## users
target_include_directories(
  cpuraytracer
  PUBLIC
    ${GLM_INCLUDE_DIRS}
    ${BVH_INCLUDE_DIR}
)
target_sources(cpuraytracer PRIVATE ${SOURCE_LIST} ${HEADER_LIST})

## install binary files
install(
  TARGETS cpuraytracer
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT core
)
