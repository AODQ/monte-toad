add_executable(monte-toad-editor)

target_sources(
  monte-toad-editor
  PRIVATE
    src/ui.cpp src/fileutil.cpp src/graphicscontext.cpp

    src/source.cpp
)

#target_compile_features(monte-toad-editor PRIVATE cxx_std_20)

set_target_properties(
  monte-toad-editor
  PROPERTIES
    COMPILE_FLAGS
      "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
       -Wundef -fno-exceptions"
)

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(
  monte-toad-editor
  PRIVATE
    monte-toad mt-plugin-host
    cxxopts glfw OpenGL::OpenGL glad imgui
    nlohmann_json::nlohmann_json
)

install(
  TARGETS monte-toad-editor
  RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)
