#ifndef ENGINE_RENDER_OPENGL_H
#define ENGINE_RENDER_OPENGL_H

namespace engine::render {

// Loads GL entry points for the context currently owned by engine_platform.
// Call once on the context thread before constructing any render object. Throws
// std::runtime_error when no compatible OpenGL loader/context is available.
void initializeOpenGl();

}  // namespace engine::render

#endif
