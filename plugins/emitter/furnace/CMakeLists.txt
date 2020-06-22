add_library(emitter-furnace SHARED)
target_sources(emitter-furnace PRIVATE src/source.cpp)

target_link_libraries(
  emitter-furnace
  PRIVATE
    mt-plugin monte-toad
    imgui
)

set_target_properties(emitter-furnace PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(emitter-furnace PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(emitter-furnace PROPERTIES PREFIX "")

install(
  TARGETS emitter-furnace
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
