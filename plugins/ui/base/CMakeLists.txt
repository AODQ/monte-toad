add_library(ui-base SHARED)
target_sources(ui-base PRIVATE src/source.cpp)

target_link_libraries(
  ui-base
  PRIVATE
    mt-plugin monte-toad-core monte-toad-util
    imgui glfw
)

set_target_properties(
  ui-base
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS ui-base
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
