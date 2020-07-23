add_library(integrator-depth SHARED)
target_sources(integrator-depth PRIVATE src/source.cpp)

target_link_libraries(
  integrator-depth
  PRIVATE
    mt-plugin monte-toad
)

set_target_properties(
  integrator-depth
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS integrator-depth
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
