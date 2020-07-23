add_library(wavefront-mtl SHARED)
target_sources(wavefront-mtl PRIVATE src/source.cpp)

target_link_libraries(
  wavefront-mtl
  PRIVATE
    mt-plugin monte-toad
    imgui
)

set_target_properties(
  wavefront-mtl
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS wavefront-mtl
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
