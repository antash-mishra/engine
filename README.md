# Mini-Engine

A simple 3D graphics engine using OpenGL.

## What is Mini-Engine?

Mini-Engine is a learning project that shows how to make basic 3D graphics with OpenGL. It helps beginners understand computer graphics concepts like shaders, textures, cameras, and lighting.

## Features

### Graphics
- 3D rendering with modern OpenGL
- Depth testing for proper 3D display
- Multiple objects with different positions and rotations

### Shader System
- Easy-to-use `Shader` class 
- Load shader code from separate files
- Simple methods to update shader values

### Textures
- `Texture` class for loading and managing images
- Support for different image formats
- Mix multiple textures together

### Camera
- `Camera` class for moving around the 3D world
- First-person controls (WASD + mouse)
- Zoom in/out with scroll wheel

### Lighting
- Phong lighting model (ambient, diffuse, specular)
- Material properties for controlling object appearance
- Moving light sources
- Directional lighting
- Point lighting with attenuation
- Lighting maps (diffuse and specular)

### Transformations
- Position, rotate, and scale 3D objects
- Model-View-Projection matrix system

## Project Progress

1. Started with basic triangle rendering
2. Added shader support with custom Shader class
3. Added texture loading and display
4. Implemented 3D transformations
5. Built camera controls for movement
6. Created dedicated Texture and Camera classes
7. Added basic lighting with a moving light source
8. Implemented material system for objects
9. Added lighting maps for detailed surface properties
10. Implemented directional and point lighting

## How to Build and Run

```bash
mkdir -p build
cd build
cmake ..
make
./game_engine
```

## Controls

- **W/A/S/D**: Move forward/left/backward/right
- **Mouse**: Look around
- **Scroll wheel**: Zoom in/out
- **ESC**: Exit

## Dependencies

- GLFW: Creates window and handles input
- GLAD: Loads OpenGL functions
- GLM: Math library for 3D graphics
- stb_image: Loads image files for textures

## Next Steps

- Add spotlights
- Implement shadow mapping
- Load 3D models from files
- Add basic physics
- Create a scene graph system