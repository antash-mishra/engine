#ifndef ENGINE_PLATFORM_WINDOW_H
#define ENGINE_PLATFORM_WINDOW_H

#include <filesystem>
#include <string>

struct GLFWwindow;

namespace engine::platform {

struct WindowConfig {
    int width = 800;
    int height = 600;
    std::string title = "Mini Engine";
    bool visible = true;
    bool enableVsync = true;
};

// Owns GLFW's process-global initialization and one OpenGL context.
//
// Only one Window may be alive at a time. All methods, construction, and
// destruction must run on the main thread. Objects that own GL handles must be
// destroyed before this window so their destructors execute with a current
// context. Construction throws std::runtime_error if GLFW or context creation
// fails; no partially initialized GLFW lifetime is retained after a failure.
class Window final {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    bool shouldClose() const;
    void requestClose();
    void pollEvents();
    void swapBuffers();
    bool escapePressed() const;
    double timeSeconds() const;
    void framebufferSize(int& width, int& height) const;

private:
    GLFWwindow* window_ = nullptr;
};

// Returns the running executable rather than the shell's working directory.
// Linux uses /proc/self/exe; the argv0 fallback is canonicalized and may throw
// when the platform cannot identify an executable.
std::filesystem::path executablePath(const char* argv0);

}  // namespace engine::platform

#endif
