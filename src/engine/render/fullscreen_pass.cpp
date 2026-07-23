#include <engine/render/fullscreen_pass.h>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace engine::render {
namespace {

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open shader: " + path.string());
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        throw std::runtime_error("failed while reading shader: " + path.string());
    }
    return contents.str();
}

unsigned int compileShader(
    unsigned int stage,
    const std::filesystem::path& path) {
    const std::string source = readTextFile(path);
    const char* sourcePointer = source.c_str();
    const unsigned int shader = glCreateShader(stage);
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);

    int compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    int logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<std::size_t>(logLength > 1 ? logLength : 1), '\0');
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error(
        "shader compilation failed for " + path.string() + ": " + log);
}

unsigned int linkProgram(unsigned int vertexShader, unsigned int fragmentShader) {
    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    int logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<std::size_t>(logLength > 1 ? logLength : 1), '\0');
    glGetProgramInfoLog(program, logLength, nullptr, log.data());
    glDeleteProgram(program);
    throw std::runtime_error("shader program link failed: " + log);
}

void setMatrix(unsigned int program, const char* name, const glm::mat4& value) {
    glUniformMatrix4fv(
        glGetUniformLocation(program, name), 1, GL_FALSE, glm::value_ptr(value));
}

}  // namespace

FullscreenPass::FullscreenPass(
    const std::filesystem::path& vertexShaderPath,
    const std::filesystem::path& fragmentShaderPath) {
    const unsigned int vertexShader =
        compileShader(GL_VERTEX_SHADER, vertexShaderPath);
    unsigned int fragmentShader = 0;
    try {
        fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderPath);
        program_ = linkProgram(vertexShader, fragmentShader);
    } catch (...) {
        glDeleteShader(vertexShader);
        if (fragmentShader != 0) {
            glDeleteShader(fragmentShader);
        }
        throw;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    constexpr float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
    };

    glGenVertexArrays(1, &vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);
}

FullscreenPass::~FullscreenPass() {
    if (vertexBuffer_ != 0) {
        glDeleteBuffers(1, &vertexBuffer_);
    }
    if (vertexArray_ != 0) {
        glDeleteVertexArrays(1, &vertexArray_);
    }
    if (program_ != 0) {
        glDeleteProgram(program_);
    }
}

void FullscreenPass::render(
    const scene::CameraState& camera,
    int framebufferWidth,
    int framebufferHeight,
    float timeSeconds) const {
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        throw std::runtime_error("cannot render a zero-sized framebuffer");
    }

    const float aspect =
        static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program_);
    setMatrix(program_, "projection", camera.projectionMatrix(aspect));
    setMatrix(program_, "view", camera.viewMatrix());
    setMatrix(program_, "model", glm::mat4(1.0f));
    glUniform3fv(
        glGetUniformLocation(program_, "cameraPosition"),
        1,
        glm::value_ptr(camera.position));
    glUniform2f(
        glGetUniformLocation(program_, "iResolution"),
        static_cast<float>(framebufferWidth),
        static_cast<float>(framebufferHeight));
    glUniform1f(glGetUniformLocation(program_, "iTime"), timeSeconds);
    glBindVertexArray(vertexArray_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

}  // namespace engine::render
