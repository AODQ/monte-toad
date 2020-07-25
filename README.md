# Monte Toad

Ongoing WIP project animation renderer. The majority of the feature-set is
plugin-based with native support for hot-reloading.

# External Dependencies

current goal is to have most external dependencies be part of the build process
  using git submodules

- ASSIMP
- OpenMP
- spdlog
- fmt

# Internal Dependencies
  (part of third-party, don't need to download)

- bvh
- stb\_image.hpp
- cxxopts
- GLM

# Support

To run the editor, OpenGL3.2 is required. Image processing can still be done
without a GPU.

Minimum of CMake 3.0 is necessary. I've only tested with GCC 10.1 and Clang
  10.0 on Linux. There is no windows support as of now.

# Current Feature List

* forward next-event-estimation path-tracer
* real-time visualizer/editor
* multiple integrator visualization
* multi-threaded
* cameras, integrators, ray dispatchers, emitters, denoisers/post-processing,
    random generators, and UI are all configurable plugins

# Future Feature List

* Windows support
* OpenImageDenoiser support
* RTX support (probably through Vulkan)
* material & layered material editor
* bidirectional path tracing
* animation support

# Installation (Linux)

Installation uses standard CMake procedure. Here is how to install it locally:

```
mkdir monte-toad
cd monte-toad
git clone https://github.com/aodq/monte-toad repo
cd repo
git submodule update --init --recursive
cd ..
mkdir build
mkdir install
cd build
cmake -DCMAKE_INSTALL_PREFIX=../install ../repo
make -j4 install
```

# Running real-time renderer
simply run monte-toad-editor; in order to use it though plugins are necessary to
be loaded. Several plugins are provided for this purpose.

# Running offline renderer
this is not currently available

# Development

current layout of how software/dependencies interact with each other is in
software-layout.dot . Most of monte-toad should be extendable with plugins
alone.
