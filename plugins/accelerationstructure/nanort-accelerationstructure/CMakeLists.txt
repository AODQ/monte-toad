add_library(nanort-accelerationstructure SHARED)
target_sources(nanort-accelerationstructure PRIVATE src/source.cpp)

target_link_libraries(
  nanort-accelerationstructure
  PRIVATE
    mt-plugin monte-toad-core
    imgui nanort
)

set_target_properties(
  nanort-accelerationstructure
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS nanort-accelerationstructure
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
