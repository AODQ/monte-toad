add_library(perfect-refractive-mtl SHARED)
target_sources(perfect-refractive-mtl PRIVATE src/source.cpp)

target_link_libraries(
  perfect-refractive-mtl
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  perfect-refractive-mtl
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS perfect-refractive-mtl
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
