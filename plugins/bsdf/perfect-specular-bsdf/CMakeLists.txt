add_library(perfect-specular-bsdf SHARED)
target_sources(perfect-specular-bsdf PRIVATE src/source.cpp)

target_link_libraries(
  perfect-specular-bsdf
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  perfect-specular-bsdf
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS perfect-specular-bsdf
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
