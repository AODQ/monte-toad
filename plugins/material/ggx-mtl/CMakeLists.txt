add_library(ggx-mtl SHARED)
target_sources(ggx-mtl PRIVATE src/source.cpp)

target_link_libraries(
  ggx-mtl
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  ggx-mtl
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS ggx-mtl
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
