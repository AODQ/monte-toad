add_library(integrator-normal SHARED)
target_sources(integrator-normal PRIVATE src/source.cpp)

target_link_libraries(
  integrator-normal
  PRIVATE
    mt-plugin monte-toad
)

set_target_properties(
  integrator-normal
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS integrator-normal
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
