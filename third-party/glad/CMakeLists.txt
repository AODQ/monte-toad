add_library(glad STATIC)

target_include_directories(glad PUBLIC "include/")
target_sources(glad PRIVATE src/glad.cpp)

find_package(OpenGL REQUIRED)

set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(glad PUBLIC OpenGL::OpenGL ${CMAKE_DL_LIBS})
