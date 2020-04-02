cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
cmake_policy(VERSION 3.5)

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

## setup proejct
set(NAME cpuraytracer)
project(${NAME} CXX)

## Add dependents
find_package(glm REQUIRED FATAL_ERROR)
find_package(OpenMP)

## source list for application (src/include)
set(SOURCE_LIST
  "src/source.cpp"
  "src/scene.cpp"
)
set(HEADER_LIST
  "src/scene.hpp"
  "src/ext/cxxopts.hpp"
)

## add exceutable w/ source list and set compile options
add_executable(cpuraytracer ${SOURCE_LIST})
target_compile_features(cpuraytracer PRIVATE cxx_std_20)

## link dependents
target_link_libraries(cpuraytracer glm spdlog fmt OpenMP::OpenMP_CXX assimp)

## add include/source directories , sources support necessary for (lamer) IDE
## users
target_include_directories(
  cpuraytracer
  PUBLIC ${GLM_INCLUDE_DIRS}
)
target_sources(cpuraytracer PRIVATE ${SOURCE_LIST} ${HEADER_LIST})

## install binary files
install(
  TARGETS cpuraytracer
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT core
)
