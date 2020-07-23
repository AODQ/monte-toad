add_library(emitter-environment SHARED)
target_sources(emitter-environment PRIVATE src/source.cpp)

target_link_libraries(
  emitter-environment
  PRIVATE
    mt-plugin monte-toad
    imgui
)

set_target_properties(
  emitter-environment
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS emitter-environment
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
