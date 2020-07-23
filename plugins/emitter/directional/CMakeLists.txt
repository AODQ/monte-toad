add_library(emitter-directional SHARED)
target_sources(emitter-directional PRIVATE src/source.cpp)

target_link_libraries(
  emitter-directional
  PRIVATE
    mt-plugin monte-toad
    imgui
)

set_target_properties(
  emitter-directional
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS emitter-directional
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
