add_library(monte-toad STATIC)

target_include_directories(monte-toad PUBLIC "include/")
target_sources(
  monte-toad
  PRIVATE
    src/imagebuffer.cpp
    src/textureloader.cpp
    src/material/layered.cpp
)

find_package(assimp REQUIRED)
find_package(OpenMP)

set_target_properties(
  monte-toad
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(
  monte-toad
  PUBLIC
    OpenMP::OpenMP_CXX assimp mt-plugin monte-toad-core
  PRIVATE
    stb
)
