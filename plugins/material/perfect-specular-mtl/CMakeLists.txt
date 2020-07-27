add_library(perfect-specular-mtl SHARED)
target_sources(perfect-specular-mtl PRIVATE src/source.cpp)

target_link_libraries(
  perfect-specular-mtl
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  perfect-specular-mtl
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS perfect-specular-mtl
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
