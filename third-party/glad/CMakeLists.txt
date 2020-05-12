add_library(glad STATIC)

target_include_directories(glad PUBLIC "include/")
target_sources(glad PRIVATE src/glad.cpp)

find_package(OpenGL REQUIRED)

set_property(TARGET glad PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(glad PUBLIC OpenGL::GL ${CMAKE_DL_LIBS})
