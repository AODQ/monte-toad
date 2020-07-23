add_library(mt-plugin INTERFACE)

target_include_directories(mt-plugin INTERFACE include/)
target_link_libraries(
  mt-plugin
  INTERFACE glm
)
