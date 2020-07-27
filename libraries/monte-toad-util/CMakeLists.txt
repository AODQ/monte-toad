add_library(monte-toad-util STATIC)
target_include_directories(monte-toad-util PUBLIC include/)
target_sources(
  monte-toad-util
  PRIVATE
    src/util/file.cpp
    src/util/textureloader.cpp
)

set_target_properties(
  monte-toad-util
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(
  monte-toad-util
  PRIVATE
    stb monte-toad-core
)
