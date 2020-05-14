add_library(integrator-forward-pt SHARED)
target_sources(integrator-forward-pt PRIVATE src/source.cpp)

target_link_libraries(
  integrator-forward-pt
  PRIVATE
    mt-plugin monte-toad
    cr
)

set_target_properties(integrator-forward-pt PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(integrator-forward-pt PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(integrator-forward-pt PROPERTIES PREFIX "")

install(
  TARGETS integrator-forward-pt
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
