#ifndef ENGINE_RENDER_FULLSCREEN_PASS_H
#define ENGINE_RENDER_FULLSCREEN_PASS_H

#include <engine/scene/camera_state.h>

#include <filesystem>

namespace engine::render {

// Owns a linked GL program and fullscreen-quad VAO/VBO for one context.
//
// Construction reads and compiles both shader files and throws with the file or
// driver log on failure. The object is neither copyable nor movable because its
// handles are context-bound. It must be destroyed on the context thread before
// engine_platform::Window. render() writes the current default framebuffer and
// assumes the caller controls its clear/swap policy.
class FullscreenPass final {
public:
    FullscreenPass(
        const std::filesystem::path& vertexShader,
        const std::filesystem::path& fragmentShader);
    ~FullscreenPass();

    FullscreenPass(const FullscreenPass&) = delete;
    FullscreenPass& operator=(const FullscreenPass&) = delete;
    FullscreenPass(FullscreenPass&&) = delete;
    FullscreenPass& operator=(FullscreenPass&&) = delete;

    void render(
        const scene::CameraState& camera,
        int framebufferWidth,
        int framebufferHeight,
        float timeSeconds) const;

private:
    unsigned int program_ = 0;
    unsigned int vertexArray_ = 0;
    unsigned int vertexBuffer_ = 0;
};

}  // namespace engine::render

#endif
