add_library(dispatcher-path-recorder SHARED)
target_sources(dispatcher-path-recorder PRIVATE src/source.cpp)

target_link_libraries(
  dispatcher-path-recorder
  PRIVATE
    mt-plugin monte-toad
)

set_target_properties(dispatcher-path-recorder PROPERTIES COMPILE_FLAGS "-Wall")
set_target_properties(dispatcher-path-recorder PROPERTIES SUFFIX ".mt-plugin")
set_target_properties(dispatcher-path-recorder PROPERTIES PREFIX "")

install(
  TARGETS dispatcher-path-recorder
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
