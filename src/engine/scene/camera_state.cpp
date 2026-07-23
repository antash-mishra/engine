#include <engine/scene/camera_state.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace engine::scene {
namespace {

bool finite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

}  // namespace

bool CameraState::validate(std::string& reason) const {
    if (!finite(position) || !finite(forward) || !finite(up)) {
        reason = "camera vectors must contain finite values";
        return false;
    }
    if (glm::length(forward) < 0.0001f || glm::length(up) < 0.0001f) {
        reason = "camera forward and up vectors must be non-zero";
        return false;
    }
    if (glm::length(glm::cross(forward, up)) < 0.0001f) {
        reason = "camera forward and up vectors must not be parallel";
        return false;
    }
    if (!std::isfinite(verticalFovDegrees) || verticalFovDegrees <= 0.0f ||
        verticalFovDegrees >= 180.0f) {
        reason = "camera vertical field of view must be between 0 and 180 degrees";
        return false;
    }
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane) ||
        nearPlane <= 0.0f || farPlane <= nearPlane) {
        reason = "camera clip planes must satisfy 0 < near < far";
        return false;
    }
    reason.clear();
    return true;
}

glm::mat4 CameraState::viewMatrix() const {
    return glm::lookAt(position, position + glm::normalize(forward), glm::normalize(up));
}

glm::mat4 CameraState::projectionMatrix(float aspectRatio) const {
    return glm::perspective(
        glm::radians(verticalFovDegrees), aspectRatio, nearPlane, farPlane);
}

}  // namespace engine::scene
