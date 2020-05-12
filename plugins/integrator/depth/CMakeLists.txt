add_library(integrator-depth SHARED)
target_sources(integrator-depth PRIVATE src/source.cpp)

target_link_libraries(
  integrator-depth
  PRIVATE
    mt-plugin monte-toad
    cr
)

set_target_properties(integrator-depth PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(integrator-depth PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(integrator-depth PROPERTIES PREFIX "")

install(
  TARGETS integrator-depth
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
