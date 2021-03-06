# Have to either call subdirectory if third-party already has a compatible
# CMakeLists.txt , otherwise create the target by-hand

add_subdirectory(bvh)
add_subdirectory(glad)
add_subdirectory(imgui)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(json)

## STB
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE stb/include)

## cxxopts
add_library(cxxopts INTERFACE)
target_compile_definitions(cxxopts INTERFACE CXXOPTS_NO_EXCEPTIONS)
target_include_directories(cxxopts INTERFACE cxxopts/include)


## GLM
add_library(glm INTERFACE)
target_include_directories(
  glm
  INTERFACE
    glm/include
)
target_precompile_headers(
  glm
  INTERFACE
    glm/include/glm/glm.hpp
    glm/include/glm/ext/matrix_transform.hpp
)

set(OIDN_STATIC_LIB ON CACHE INTERNAL "")
set(OIDN_STATIC_RUNTIME OFF CACHE INTERNAL "")
set(OIDN_APPS OFF CACHE INTERNAL "")
set(OIDN_APPS_OPENIMAGEIO OFF CACHE INTERNAL "")
add_subdirectory(open-image-denoise)

## spdlog
set(SPDLOG_BUILD_SHARED ON CACHE INTERNAL "")
set(SPDLOG_ENABLE_PCH ON CACHE INTERNAL "")
set(SPDLOG_NO_EXCEPTIONS ON CACHE INTERNAL "")
set(SPDLOG_INSTALL ON CACHE INTERNAL "")
add_subdirectory(spdlog)

## nanort
add_library(nanort INTERFACE)
target_include_directories(nanort INTERFACE nanort/include)
