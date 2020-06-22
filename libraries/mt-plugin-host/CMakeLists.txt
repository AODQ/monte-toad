add_library(mt-plugin-host STATIC)

target_include_directories(mt-plugin-host PUBLIC "include/")
target_sources(mt-plugin-host PRIVATE src/plugin.cpp)

set_target_properties(mt-plugin-host PROPERTIES COMPILE_FLAGS "-Wall")

target_link_libraries(mt-plugin-host PUBLIC mt-plugin)
