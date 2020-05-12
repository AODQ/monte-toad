# Monte Toad

Ongoing WIP project animation renderer

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

# Installation (Linux)

Installation is trivial if you have used CMake before. For redundancy, here is how to install it locally:

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
