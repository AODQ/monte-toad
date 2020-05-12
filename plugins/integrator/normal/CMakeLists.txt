add_library(integrator-normal SHARED)
target_sources(integrator-normal PRIVATE src/source.cpp)

target_link_libraries(
  integrator-normal
  PRIVATE
    mt-plugin monte-toad
    cr
)

set_target_properties(integrator-normal PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(integrator-normal PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(integrator-normal PROPERTIES PREFIX "")

install(
  TARGETS integrator-normal
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
