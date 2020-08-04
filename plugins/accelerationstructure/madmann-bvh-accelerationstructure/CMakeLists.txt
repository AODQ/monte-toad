add_library(madmann-bvh-accelerationstructure SHARED)
target_sources(madmann-bvh-accelerationstructure PRIVATE src/source.cpp)

target_link_libraries(
  madmann-bvh-accelerationstructure
  PRIVATE
    mt-plugin monte-toad-core
    imgui bvh
)

set_target_properties(
  madmann-bvh-accelerationstructure
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef -fno-exceptions"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS madmann-bvh-accelerationstructure
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
