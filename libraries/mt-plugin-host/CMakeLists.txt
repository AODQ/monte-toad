add_library(mt-plugin-host STATIC)

target_include_directories(mt-plugin-host PUBLIC "include/")
target_sources(mt-plugin-host PRIVATE src/plugin.cpp)

target_compile_definitions(
  mt-plugin-host
  PUBLIC
    SPDLOG_NO_EXCEPTIONS=1
    SPDLOG_FMT_EXTERNAL=1
)

set_target_properties(
  mt-plugin-host
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
)

target_link_libraries(mt-plugin-host PUBLIC mt-plugin spdlog)
