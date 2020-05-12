add_library(camera-pinhole SHARED)
target_sources(camera-pinhole PRIVATE src/source.cpp)

target_link_libraries(
  camera-pinhole
  PRIVATE
    mt-plugin monte-toad
    cr
)

set_target_properties(camera-pinhole PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(camera-pinhole PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(camera-pinhole PROPERTIES PREFIX "")

install(
  TARGETS camera-pinhole
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
