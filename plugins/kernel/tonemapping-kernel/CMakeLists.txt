add_library(tonemapping-kernel SHARED)
target_sources(tonemapping-kernel PRIVATE src/source.cpp)

target_link_libraries(
  tonemapping-kernel
  PRIVATE
    mt-plugin monte-toad-core imgui
)

set_target_properties(
  tonemapping-kernel
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS tonemapping-kernel
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
