add_library(open-image-denoiser-kernel SHARED)
target_sources(open-image-denoiser-kernel PRIVATE src/source.cpp)

target_link_libraries(
  open-image-denoiser-kernel
  PRIVATE
    mt-plugin monte-toad-core
    OpenImageDenoise
)

set_target_properties(
  open-image-denoiser-kernel
    PROPERTIES
      COMPILE_FLAGS
        "-Wshadow -Wdouble-promotion -Wall -Wformat=2 -Wextra -Wpedantic \
         -Wundef"
      SUFFIX ".mt-plugin"
      PREFIX ""
)

install(
  TARGETS open-image-denoiser-kernel
  LIBRARY NAMELINK_SKIP
  LIBRARY
    DESTINATION plugins/
    COMPONENT plugin
)
