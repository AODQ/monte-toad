add_library(ui-base SHARED)
target_sources(ui-base PRIVATE src/source.cpp)

target_link_libraries(
  ui-base
  PRIVATE
    mt-plugin monte-toad
    cr imgui
)

set_target_properties(ui-base PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(ui-base PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(ui-base PROPERTIES PREFIX "")

install(
  TARGETS ui-base
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
