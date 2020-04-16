# CPU raytracer

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
mkdir cpu-raytracer
cd cpu-raytracer
git clone https://github.com/aodq/cpu-raytracer repo
cd repo
git submodule update --init --recursive
cd ..
mkdir build
mkdir install
cd build
cmake -DCMAKE_INSTALL_PREFIX=../install ../repo
make -j4 install
```

# Running
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
