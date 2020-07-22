# Monte Toad

Ongoing WIP project animation renderer. The majority of the feature-set is
C-ABI plugin-based with native support for hot-reloading.

# External Dependencies

- ASSIMP
- GLM
- OpenMP
- spdlog
- fmt

# Internal Dependencies
  (part of third-party, don't need to download)

- bvh
- stb\_image.hpp
- cxxopts

# Support

To run the editor, OpenGL3.2 is required. Image processing can still be done
without a GPU.

Minimum of CMake 3.0 is necessary

The following compilers/toolsets are tested/supported:

  * GCC
  * Clang
  * ccache

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
There are several shell scripts provided under scripts that should assist in
default scene configurations. (In the future these will probably be json files).

As an example to render the BMW (with environmental spherical map)

```
git clone https://github.com/aodq/test-models
cd test-models
./extract.sh
cd ..
./scripts/bmw-front.sh -e ~/spherical-texture.png
feh --force-aliasing out.ppm
```
