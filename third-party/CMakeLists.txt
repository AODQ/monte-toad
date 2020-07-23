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
