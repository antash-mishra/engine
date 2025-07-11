# Mini-Engine

A comprehensive 3D graphics engine showcasing advanced real-time rendering techniques with OpenGL 4.3.

## What is Mini-Engine?

Mini-Engine is an advanced graphics learning project that demonstrates modern real-time rendering techniques. It has evolved from basic triangle rendering to a sophisticated engine featuring PBR materials, shadow mapping, deferred rendering, and cluster-based lighting optimizations.

## Features

### Core Graphics Pipeline
- **OpenGL 4.3** with modern core profile
- **Deferred rendering** for efficient multi-light scenes
- **Cluster-based deferred shading** supporting 10,000+ lights with zero FPS drop
- **Compute shaders** for cluster grid calculations and light culling

### Advanced Lighting & Shading
- **Physically Based Rendering (PBR)** with metallic-roughness workflow
- **Image Based Lighting (IBL)** with:
  - Diffuse irradiance mapping
  - Specular reflection with prefiltered environment maps
  - BRDF integration lookup table
- **Direct lighting**: Directional, point, and spot lights
- **Blinn-Phong** and **PBR BRDF** shading models

### Shadow Mapping
- **Real-time shadow mapping** with PCF (Percentage Closer Filtering)
- **Point light shadows** using geometry shaders and cube maps
- **Directional light shadows** with orthographic projection
- **Shadow acne elimination** and bias correction
- **Smooth shadow edges** with multi-sample filtering

### Post-Processing Effects
- **HDR rendering** with tone mapping operators
- **Bloom effect** with gaussian blur
- **SSAO (Screen Space Ambient Occlusion)** for realistic ambient shading
- **Gamma correction** for proper color space handling

### Asset Loading & Scene Management
- **glTF 2.0 model loading** using TinyGLTF
- **KHR_lights_punctual** extension support for scene lighting
- **Sponza scene** with full material and lighting setup
- **Multiple texture support** (albedo, normal, metallic-roughness)

### Rendering Optimizations
- **Frustum culling** and **light volume optimization**
- **Face culling** and **depth testing**
- **Instanced rendering** for repeated geometry
- **Light volume spheres** for deferred rendering optimization

### Technical Features
- **Advanced shader system** with uniform management
- **Framebuffer objects** for multi-pass rendering
- **Cubemap support** for skyboxes and environment mapping
- **Dynamic window resizing** with proper viewport management
- **Debug visualization** for shadow maps and light volumes

## Rendering Techniques Demonstrated

1. **Forward Rendering** → **Deferred Rendering** → **Cluster Deferred Rendering**
2. **Basic Lighting** → **PBR Materials** → **IBL Environment Lighting**
3. **No Shadows** → **Shadow Maps** → **Multi-light Shadows**
4. **LDR** → **HDR** → **Tone Mapping & Bloom**
5. **Simple Geometry** → **Complex Models** → **glTF Scene Loading**

## Project Evolution (Latest Commits)

- **Current**: Sponza scene with directional lighting and shadow mapping
- **PBR & IBL**: Full physically-based materials with environment lighting
- **SSAO**: Screen-space ambient occlusion for realistic depth perception
- **Cluster Shading**: 10,000+ light support with compute shader optimization
- **Deferred Pipeline**: Multi-pass rendering for complex lighting scenarios
- **Shadow Systems**: Real-time shadows for directional and point lights
- **HDR Pipeline**: High dynamic range with bloom and tone mapping

## How to Build and Run

### Prerequisites
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential cmake libglfw3-dev libglew-dev libglm-dev
```

### Build
```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### Run Different Demos
```bash
# Main Sponza scene with PBR and shadows
./sponza

# Deferred rendering demo
./deffered_rendering

# Cluster deferred shading (10k lights)
./cluster_deffered_rendering

# HDR and bloom demo
./hdr

# Basic lighting examples
./lighting_main
```

## Controls

### Camera
- **W/A/S/D**: Move forward/left/backward/right
- **Mouse**: Look around (FPS-style)
- **Scroll wheel**: Zoom in/out
- **Tab**: Toggle mouse cursor for UI interaction

### Debug Controls
- **L**: Toggle light cube visualization
- **O**: Toggle SSAO debug view (where available)
- **ESC**: Exit application

## Scene Assets

### Sponza Scene
- **Location**: `resources/main-sponza/main_sponza/`
- **Format**: glTF 2.0 with PBR materials
- **Features**: 
  - High-quality textures (albedo, normal, metallic-roughness)
  - Scene lighting with KHR_lights_punctual
  - Complex architecture for shadow testing

### Environment Maps
- **HDR Environment**: `resources/newport_loft.hdr`
- **Skybox textures**: `resources/skybox/`

## Technical Architecture

### Shaders
- **Vertex/Fragment**: Traditional pipeline stages
- **Geometry**: Point light shadow mapping
- **Compute**: Cluster grid generation and light culling

### Framebuffers
- **G-Buffer**: Position, normal, albedo, material properties
- **Shadow Maps**: Depth textures for shadow calculations
- **HDR Buffer**: High dynamic range color storage
- **Bloom**: Multi-pass gaussian blur

### Dependencies

- **GLFW 3.3+**: Window management and input
- **GLAD**: OpenGL function loading
- **GLM**: Mathematics library for graphics
- **stb_image**: Image loading (PNG, JPG, HDR)
- **TinyGLTF**: glTF 2.0 model loading
- **Dear ImGui**: Debug UI (in some demos)

## Performance Notes

- **Cluster Deferred**: Handles 10,000+ lights at 60+ FPS
- **Shadow Maps**: 1024x1024 depth maps with PCF
- **SSAO**: Full-screen effect with configurable samples
- **PBR**: Real-time IBL with precomputed environment maps

## Future Enhancements

- **Temporal Anti-Aliasing (TAA)**
- **Screen Space Reflections (SSR)**
- **Volumetric lighting and fog**
- **Animation system for glTF models**
- **Real-time global illumination**

## Learning Resources

Each major technique is implemented in separate demos, making it easy to understand the progression from basic to advanced rendering. Check the `docs/` directory for detailed explanations of specific techniques.

---

*This project demonstrates the evolution from basic OpenGL rendering to production-quality real-time graphics techniques.*