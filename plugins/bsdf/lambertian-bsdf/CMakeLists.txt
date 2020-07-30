add_library(lambertian-mtl SHARED)
target_sources(lambertian-mtl PRIVATE src/source.cpp)

target_link_libraries(
  lambertian-mtl
  PRIVATE
    mt-plugin monte-toad-core
    imgui
)

set_target_properties(
  lambertian-mtl
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS lambertian-mtl
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
