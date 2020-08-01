add_library(base-material SHARED)
target_sources(base-material PRIVATE src/source.cpp)

target_link_libraries(
  base-material
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  base-material
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS base-material
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
