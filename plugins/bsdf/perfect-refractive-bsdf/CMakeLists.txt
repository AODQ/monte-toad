add_library(perfect-refractive-bsdf SHARED)
target_sources(perfect-refractive-bsdf PRIVATE src/source.cpp)

target_link_libraries(
  perfect-refractive-bsdf
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  perfect-refractive-bsdf
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS perfect-refractive-bsdf
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
