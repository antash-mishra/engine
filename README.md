# Mini-Engine

A lightweight 3D graphics engine built with OpenGL.

## Overview

Mini-Engine is a simple 3D rendering engine that demonstrates fundamental computer graphics concepts using modern OpenGL. It provides a foundation for learning graphics programming with features like shader management, texture mapping, 3D transformations, and camera controls.

## Current Features

### Rendering
- Modern OpenGL (3.3+) rendering pipeline
- Vertex and fragment shader support
- Index buffer rendering for optimized mesh display
- Depth testing for proper 3D rendering

### Shader System
- Custom `Shader` class for easy shader program management
- Simplified uniform setting with typed methods (floats, vectors, matrices)
- External GLSL shader file loading

### Textures
- Texture loading via stb_image
- Multi-texture support with texture units
- UV coordinate mapping
- Texture mixing/blending

### Transformations
- 3D transformations (translation, rotation, scaling)
- Model-View-Projection matrix implementation
- GLM math library integration

### Camera
- First-person camera controls (WASD movement)
- Mouse look controls
- Field of view adjustment with scroll wheel
- Camera position and orientation vectors

## Development Progress

The project has evolved through several stages:

1. **Initial Setup**: Basic OpenGL triangle rendering
2. **Shader Implementation**: Created shader class to manage GLSL programs
3. **Texture Support**: Added texture loading and mapping
4. **Transformations**: Implemented matrix transformations for 3D objects
5. **Multiple Objects**: Rendering multiple 3D objects with different transformations
6. **Camera System**: Implemented first-person camera controls

## Next Steps

### Camera Class
- Create a dedicated `Camera` class to encapsulate camera functionality
- Implement different camera types (FPS, orbital, etc.)
- Add camera movement smoothing and constraints

### Texture Class
- Develop a `Texture` class to manage texture loading and configuration
- Support for different texture types (2D, cubemaps)
- Implement texture caching for better performance

### Future Enhancements
- Lighting system (ambient, diffuse, specular)
- Model loading (.obj, .fbx)
- Simple physics
- Scene graph
- Material system

## Building and Running

The project uses CMake as its build system:

```bash
mkdir -p build
cd build
cmake ..
make
./game_engine
```

## Controls

- **W/A/S/D**: Move camera forward/left/backward/right
- **Mouse**: Look around
- **Scroll wheel**: Adjust field of view (zoom)
- **ESC**: Exit application

## Dependencies

- GLFW: Window and input management
- GLAD: OpenGL function loader
- GLM: Mathematics library
- stb_image: Image loading
