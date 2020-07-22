add_library(ggx-mtl SHARED)
target_sources(ggx-mtl PRIVATE src/source.cpp)

target_link_libraries(
  ggx-mtl
  PRIVATE
    mt-plugin monte-toad
    imgui
)

set_target_properties(ggx-mtl PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(ggx-mtl PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(ggx-mtl PROPERTIES PREFIX "")

install(
  TARGETS ggx-mtl
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
