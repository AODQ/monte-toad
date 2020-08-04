add_library(emitter-furnace SHARED)
target_sources(emitter-furnace PRIVATE src/source.cpp)

target_link_libraries(
  emitter-furnace
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  emitter-furnace
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS emitter-furnace
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
