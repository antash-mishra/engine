#include <engine/sample/sample.h>

#include <engine/platform/window.h>
#include <engine/render/fullscreen_pass.h>
#include <engine/render/opengl.h>
#include <engine/scene/camera_state.h>

#include <stdexcept>

namespace engine::sample {
namespace {

const SampleDescriptor& rayMarchingDescriptor() {
    static const SampleDescriptor value{
        "ray-marching",
        "Procedural signed-distance-field ray marcher",
    };
    return value;
}

class RayMarchingSample final : public Sample {
public:
    const SampleDescriptor& descriptor() const noexcept override {
        return rayMarchingDescriptor();
    }

    SampleRunResult run(const SampleRunConfig& config) override {
        std::string cameraError;
        const scene::CameraState camera;
        if (!camera.validate(cameraError)) {
            throw std::runtime_error("invalid ray-marching camera: " + cameraError);
        }

        const bool fixedFrameRun = config.frameLimit.has_value();
        platform::Window window({
            config.width,
            config.height,
            "Ray Marching",
            !fixedFrameRun,
            !fixedFrameRun,
        });
        render::initializeOpenGl();

        // Destruction order matters: pass releases GL objects before window
        // destroys the context at the end of this scope.
        render::FullscreenPass pass(
            config.assetRoot.file("shaders/vertexCube.glsl"),
            config.assetRoot.file("shaders/fragmentCube.glsl"));

        SampleRunResult result;
        while (!window.shouldClose()) {
            if (fixedFrameRun && result.framesRendered >= *config.frameLimit) {
                break;
            }
            if (!fixedFrameRun && window.escapePressed()) {
                window.requestClose();
                continue;
            }

            int framebufferWidth = 0;
            int framebufferHeight = 0;
            window.framebufferSize(framebufferWidth, framebufferHeight);
            const float time = fixedFrameRun
                ? static_cast<float>(result.framesRendered) / 60.0f
                : static_cast<float>(window.timeSeconds());
            pass.render(camera, framebufferWidth, framebufferHeight, time);
            window.swapBuffers();
            window.pollEvents();
            ++result.framesRendered;
        }

        if (fixedFrameRun && result.framesRendered != *config.frameLimit) {
            throw std::runtime_error(
                "window closed before the requested frame limit was rendered");
        }
        return result;
    }
};

}  // namespace

std::unique_ptr<Sample> createRayMarchingSample() {
    return std::make_unique<RayMarchingSample>();
}

SampleRegistration rayMarchingSampleRegistration() {
    return {rayMarchingDescriptor(), &createRayMarchingSample};
}

}  // namespace engine::sample
