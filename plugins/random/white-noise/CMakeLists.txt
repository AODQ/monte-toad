add_library(white-noise SHARED)
target_sources(white-noise PRIVATE src/source.cpp)

find_package(glm REQUIRED FATAL_ERROR)

target_link_libraries(
  white-noise
  PRIVATE
    mt-plugin glm cr
)

set_target_properties(white-noise PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(white-noise PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(white-noise PROPERTIES PREFIX "")

install(
  TARGETS white-noise
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
