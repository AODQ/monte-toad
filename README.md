# CPU raytracer

# Dependencies

- ASSIMP
- GLM
- OpenMP
- spdlog
- fmt

# Installation (Linux)

There is nothing non-trivial here, but for redundancy purposes, here is how to install it locally:

```
mkdir cpu-raytracer
cd cpu-raytracer
git clone https://github.com/aodq/cpu-raytracer repo
mkdir build
mkdir install
cd build
cmake -DCMAKE_INSTALL_PREFIX=../install ../repo
make -j4 install
```
