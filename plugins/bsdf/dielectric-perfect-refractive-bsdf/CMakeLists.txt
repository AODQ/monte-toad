add_library(dielectric-perfect-refractive-bsdf SHARED)
target_sources(dielectric-perfect-refractive-bsdf PRIVATE src/source.cpp)

target_link_libraries(
  dielectric-perfect-refractive-bsdf
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  dielectric-perfect-refractive-bsdf
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS dielectric-perfect-refractive-bsdf
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
