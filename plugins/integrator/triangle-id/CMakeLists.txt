add_library(integrator-triangle-id SHARED)
target_sources(integrator-triangle-id PRIVATE src/source.cpp)

target_link_libraries(
  integrator-triangle-id
  PRIVATE
    mt-plugin monte-toad-core
)

set_target_properties(
  integrator-triangle-id
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS integrator-triangle-id
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
