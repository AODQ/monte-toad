add_library(imgui SHARED)

target_include_directories(imgui PUBLIC "include/")
target_sources(
  imgui
  PRIVATE
    src/imgui.cpp
    src/imgui_demo.cpp
    src/imgui_draw.cpp
    src/imgui_impl_glfw.cpp
    src/imgui_impl_opengl3.cpp
    src/imgui_widgets.cpp
)

target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

find_package(OpenGL REQUIRED)

target_link_libraries(imgui PUBLIC OpenGL::OpenGL glad)

install(
  TARGETS imgui
  LIBRARY
    DESTINATION "lib/"
    COMPONENT core
)
