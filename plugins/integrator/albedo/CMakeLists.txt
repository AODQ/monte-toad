add_library(integrator-albedo SHARED)
target_sources(integrator-albedo PRIVATE src/source.cpp)

target_link_libraries(
  integrator-albedo
  PRIVATE
    mt-plugin monte-toad
    imgui
)

set_target_properties(integrator-albedo PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(integrator-albedo PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(integrator-albedo PROPERTIES PREFIX "")

install(
  TARGETS integrator-albedo
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
