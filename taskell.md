## features

- move environment map file to be loaded by emitter plugin
- allow initial and post initial integrator rules (which will allow quick full image passes followed by focused rendering)
- image zooming / panning controls
- have line / square that displays current dispatch block (both global and local)
- have layers which would allow setups to switch quickly between realtime, interactive and offline modes
- save all integrators to a single PNG, properly spaced out and possibly with subtitles
- camera controls sperate from plugin info ui window
- add ggx brdf
- add ggx btdf
- subsurface scattering bssrdf
- implement bumpmapping
- when plugin fails to load, give specific information such as which function was not present etc
- investigate adding embree as an acceleration structure plugin
- investigate adding radeon rays as an acceleration structure plugin
- have plugin for intersectors (for triangles, spheres, instances, etc etc)
- saved material/bsdf preview
- give option to not update images for longer AFK renders
- add flashing text when rendering & trying to move, also add some flashing indication when rendering
- RTX support without relying on radeon rays or other libraries (would probably be implemented well after other libraries are implemented tho)
- move most third party dependencies to be compiled from source in third-party/
- precompiled header for STL
- fix layered material support system
- save image from a specific integrator
- add subdivision surface support
- add instancing support
- allow saving/loading of scene w json
- iterator spans out in a spiral, first iteration forces an iteration of 1 thus allowing for a very quick preview mode
- store override imgui resolution
- bumpmap/normal implementation
- imgui 4k support / dpi scaling / font scaling
- imgui custom text / font support
- don't halt thread while waiting for tasks to finish (probably have to replace openmp)
- fix pathtraced debug lines
- move spdlog out from monte toad core
- use glTexSubImage2D with pixen unpack buffers for faster image upload
- use mouse wheel to control camera speed
- texture viewer for editor/ui
- have json not crash on error
- framebuffer fragment inspection
- environmental map lighting loads / stores enviornment image separately
- desynced camera movement between integrators
- allow multiple integrators of the same type
- figure a way to allow mesh emitters to be a plugin (emitter plugin)
- load scene in seperate thread, give loader information about current scene loading statistics
- cross platform file navigator (imgui implementation)
- framebuffer path history inspection
- display batched rays in buffer, allowing to debug/visually see how a BRDF/BTDF effects rays in the scene.
- embed scene format as JSON (this will probably always be a WIP as even more and more features are added, but no overall design has been laid out so that all plugins may interact with json files)
- set imgui default layout
- add wavelength / spectral rendering support
- blue noise random generator
- windows support
- animation skinning / bone support
- spatiotemporal variance guided filter (SVGF) kernel
- bidirectional path tracing integrator
- scene animations (key model matrices quaternions etc)
- animate out video using FFMPEG & moving camera
- plugins can allocate memory on host
- allow multiple directional emitters w/ skybox
- every kernel has its own image transition buffer, for debug, otherwise there is (raw -> preview) or (raw -> kernel -> preview) buffers, as preview must always be the last operation that occurs
- plugins for iterating dispatchers
- allow ~20 frames after activity to update properly, otherwise the 1000 ms latency is misleading for a lot of UI options
- swap to std vector of pixels for a specific region  - this happens already but only on the entire framebuffer, this behavior should be stripped out though since it only makes sense to apply this non a non-preview render

## bugs / code cleanup

- allow acceleration structure to load the mesh from a more generic structure (ei to allow face support)
- lag spike when material is null/no bsdfs, should be resolved
- don't reload scene when loading up environment texture
- move mt::Valid for plugin valid to mt::PluginValid
- move span to mt::core::span
- have surface store refractedNormal, which is the normal but pointed in the direction necessary for refraction
- have albedo / normal integrators work with reflection/refraction
    > reflection/refraction should be a fresnel blend, but if diffuse is present then I assume none of these should be used. Probably have to experiment. This should improve open image denoiser and other filtering kernels potentially
- store cosThetaI in surface (no longer need to compute it)
- move bsdfsampleinfo to its own file
- rename mtplugin to mt::plugin along with moving all the mt::Plugin* stuff to mt::plugin::*
- better previews for 4k and 8k resolution (maybe cheap upsampling or something not sure)
- rename plugins to type-name
- fix namespace / function separation
- fix texture errors on startup
- up axis should also change the environment map sampling
- remove unused [[maybe_unused]]
- fix texture pointers crashing on texture resize (the textures array not texture dimensions)
- materials duplicate on scene load
    > q

## upcoming

- need a proper line rendering engine (for raycasts, to show regions of image currently being rendered on, etc)
- plugin reload crashes, something to do with openImageDenoiser.
- fix importance sampling (was disabled temporarily)

## in progress

- tonemapping/hdr kernels

## done

- basic hot reloading system
- switch all calloc/malloc/free to new/delete
- allow materials to be composed from brdf plugins
- openimagedenoiser support
- delete should reset itnegrator
- first-pass integrators; optimization where integrators that only need one pass and share same camera information and resolution as other integrators can share the same dispatch thus eliminating the need for redundant BVH-ray intersections
- add BRDF plugins
- fix acceleration structure (it produces degenerate triangles)
- add russian roulette
- texture/vec3/float optional / ui stable
- verify & fix rendered image being flipped on X axis
- YU plugin chain, probably useful for kernel too
- dispatcher plugin runs actual dispatches
- move material selection code etc to base ui, brdf should have dispatch for its specific properties
- rename mtl/material plugin to brdf
- optimize build times
- real-time depth integrator
- remove unused variables from functions
- allow multiple emitters (ei skyboxes)
- blocks can have stride per element
- texture support for scenes
- atmosphere emitter
- replace plugin library
- forward NEE integrator
- real-time rasterized-based-shading integrator
- environment map lighting
- plugin material system
- R key toggles rendering
- reload plugin button
- control stride lengths
- samples are skipped when a block is finished
- image is finished when all samples / blocks are done
- progressive upsampling-resolution rendering
- remove copies of plugin on cmake install
- use gold linker
- real-time normal integrator
- multiple viewers
- white-noise random generator
- pinhole camera anti aliasing
- configurable BVH / acceleration options
- fix triangle ray intersection
- have plugin for acceleration structure
- log ui element
- plugin error check (probably set an enum with its type)
- resize imgui texture & buffer sizes in realtime and independently
- freeform camera movement
- controllable flyout camera in UI
- sometimes plugins unload themselves when other plugins load
- optional imgui integration with all plugins
- openmp does not work properly when using nanort acceleration structure
- better kernel control (can control when/how and what order kernels are dispatched)
- at resolutions lower than 256 have higher strides
- give % completion of a rendered image
- display rendering time of a frame (total)
- kernels will automatically generate and cache necessary generator info (albedo/normal as same resolution as this etc)
- selective box integration
- plugins can deallocate memory from std::core::Any by passing a dealloc fn pointer (right now memory is just leaked)
