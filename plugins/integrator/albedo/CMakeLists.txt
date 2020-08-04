add_library(integrator-albedo SHARED)
target_sources(integrator-albedo PRIVATE src/source.cpp)

target_link_libraries(
  integrator-albedo
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  integrator-albedo
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS integrator-albedo
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
