add_executable(monte-toad-editor)

target_sources(
  monte-toad-editor
  PRIVATE
    src/glutil.hpp src/ui.hpp
    src/glutil.cpp src/ui.cpp

    src/source.cpp
)

target_compile_features(monte-toad-editor PRIVATE cxx_std_20)

set_target_properties(monte-toad-editor PROPERTIES COMPILE_FLAGS "-Wall")

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(
  monte-toad-editor
  PRIVATE
    monte-toad mt-plugin-host
    cxxopts glfw OpenGL::OpenGL glad imgui
)

install(
  TARGETS monte-toad-editor
  RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)
