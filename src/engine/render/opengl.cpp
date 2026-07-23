#include <engine/render/opengl.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace engine::render {

void initializeOpenGl() {
    if (gladLoadGLLoader(
            reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
        throw std::runtime_error(
            "failed to load OpenGL entry points for the current GLFW context");
    }
    if (GLAD_GL_VERSION_4_3 == 0) {
        throw std::runtime_error("OpenGL 4.3 is required by the ray-marching shaders");
    }
}

}  // namespace engine::render
