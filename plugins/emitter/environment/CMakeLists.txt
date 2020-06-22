add_library(emitter-environment SHARED)
target_sources(emitter-environment PRIVATE src/source.cpp)

target_link_libraries(
  emitter-environment
  PRIVATE
    mt-plugin monte-toad
    imgui
)

set_target_properties(emitter-environment PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(emitter-environment PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(emitter-environment PROPERTIES PREFIX "")

install(
  TARGETS emitter-environment
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
