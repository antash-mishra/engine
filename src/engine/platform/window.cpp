#include <engine/platform/window.h>

#include <GLFW/glfw3.h>

#include <system_error>
#include <stdexcept>

namespace engine::platform {
namespace {

bool windowIsAlive = false;

std::runtime_error glfwError(const std::string& operation) {
    const char* description = nullptr;
    const int code = glfwGetError(&description);
    return std::runtime_error(
        operation + " failed (GLFW error " + std::to_string(code) + "): " +
        (description == nullptr ? "no driver message" : description));
}

}  // namespace

Window::Window(const WindowConfig& config) {
    if (windowIsAlive) {
        throw std::logic_error("only one engine::platform::Window may be alive");
    }
    if (config.width <= 0 || config.height <= 0) {
        throw std::invalid_argument("window dimensions must be positive");
    }
    if (glfwInit() != GLFW_TRUE) {
        throw glfwError("glfwInit");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, config.visible ? GLFW_TRUE : GLFW_FALSE);

    window_ = glfwCreateWindow(
        config.width, config.height, config.title.c_str(), nullptr, nullptr);
    if (window_ == nullptr) {
        const std::runtime_error error = glfwError("glfwCreateWindow");
        glfwTerminate();
        throw error;
    }

    windowIsAlive = true;
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(config.enableVsync ? 1 : 0);
}

Window::~Window() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (windowIsAlive) {
        windowIsAlive = false;
        glfwTerminate();
    }
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void Window::requestClose() {
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::swapBuffers() {
    glfwSwapBuffers(window_);
}

bool Window::escapePressed() const {
    return glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS;
}

double Window::timeSeconds() const {
    return glfwGetTime();
}

void Window::framebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(window_, &width, &height);
}

std::filesystem::path executablePath(const char* argv0) {
    std::error_code error;
#if defined(__linux__)
    const auto procPath = std::filesystem::canonical("/proc/self/exe", error);
    if (!error) {
        return procPath;
    }
#endif
    if (argv0 == nullptr || *argv0 == '\0') {
        throw std::runtime_error("cannot resolve executable path: argv[0] is empty");
    }

    const auto fallback = std::filesystem::canonical(argv0, error);
    if (error) {
        throw std::runtime_error(
            "cannot resolve executable path from argv[0]: " + error.message());
    }
    return fallback;
}

}  // namespace engine::platform
