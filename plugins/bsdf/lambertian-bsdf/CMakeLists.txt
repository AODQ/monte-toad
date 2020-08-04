add_library(lambertian-bsdf SHARED)
target_sources(lambertian-bsdf PRIVATE src/source.cpp)

target_link_libraries(
  lambertian-bsdf
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  lambertian-bsdf
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS lambertian-bsdf
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
