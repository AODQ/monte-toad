add_library(wavefront-mtl SHARED)
target_sources(wavefront-mtl PRIVATE src/source.cpp)

target_link_libraries(
  wavefront-mtl
  PRIVATE
    mt-plugin monte-toad
    cr imgui
)

set_target_properties(wavefront-mtl PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(wavefront-mtl PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(wavefront-mtl PROPERTIES PREFIX "")

install(
  TARGETS wavefront-mtl
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
