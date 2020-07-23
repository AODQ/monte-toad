add_library(white-noise SHARED)
target_sources(white-noise PRIVATE src/source.cpp)

target_link_libraries(
  white-noise
  PRIVATE
    monte-toad mt-plugin
    glm
)

set_target_properties(
  white-noise
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS white-noise
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
