add_library(camera-pinhole SHARED)
target_sources(camera-pinhole PRIVATE src/source.cpp)

target_link_libraries(
  camera-pinhole
  PRIVATE
    mt-plugin monte-toad-core
)

set_target_properties(
  camera-pinhole
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS camera-pinhole
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
