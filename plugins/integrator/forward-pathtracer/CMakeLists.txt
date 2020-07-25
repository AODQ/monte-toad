add_library(integrator-forward-pt SHARED)
target_sources(integrator-forward-pt PRIVATE src/source.cpp)

target_link_libraries(
  integrator-forward-pt
  PRIVATE
    mt-plugin monte-toad-core monte-toad-debug-util
)

set_target_properties(
  integrator-forward-pt
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS integrator-forward-pt
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
