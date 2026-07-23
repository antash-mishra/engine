#ifndef ENGINE_SCENE_CAMERA_STATE_H
#define ENGINE_SCENE_CAMERA_STATE_H

#include <glm/glm.hpp>

#include <string>

namespace engine::scene {

// Renderer-independent camera state passed by value between a sample and render
// passes. validate() reports malformed finite/range/vector inputs before a GL
// call. Matrix methods do not retain references and are thread-safe.
struct CameraState {
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float verticalFovDegrees = 35.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    bool validate(std::string& reason) const;
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspectRatio) const;
};

}  // namespace engine::scene

#endif
