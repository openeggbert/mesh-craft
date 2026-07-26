#include "MeshCraft/Editor/EditorCamera.hpp"
#include "MeshCraft/EditorTransformAlgorithms.hpp"

#include <Microsoft/Xna/Framework/MathHelper.hpp>
#include <algorithm>
#include <cmath>

using namespace Microsoft::Xna::Framework;

namespace MeshCraft::Editor {

EditorCamera::EditorCamera() = default;

void EditorCamera::orbit(float deltaYaw, float deltaPitch) {
    yaw   += deltaYaw;
    pitch += deltaPitch;
    pitch  = std::clamp(pitch, kPitchMin, kPitchMax);
}

void EditorCamera::pan(float deltaX, float deltaY) {
    // Move target in the camera's right and up directions
    float sinYaw = std::sin(yaw),   cosYaw = std::cos(yaw);
    float sinPitch = std::sin(pitch);

    // Camera right vector (in XZ plane rotated by yaw)
    Vector3 right{ cosYaw, 0.0f, -sinYaw };
    // Camera up in world space (approximation good for edit camera)
    Vector3 up{ sinYaw * sinPitch, std::cos(pitch), cosYaw * sinPitch };

    float panScale = distance * 0.001f;
    target.X += (right.X * deltaX + up.X * deltaY) * panScale;
    target.Y += (right.Y * deltaX + up.Y * deltaY) * panScale;
    target.Z += (right.Z * deltaX + up.Z * deltaY) * panScale;
}

void EditorCamera::zoom(float delta) {
    distance *= std::exp(-delta * 0.1f);
    distance  = std::clamp(distance, kDistMin, kDistMax);
}

void EditorCamera::focusOn(float x, float y, float z, float radius) {
    target = { x, y, z };
    distance = std::max(radius * 3.0f, kDistMin);
}

void EditorCamera::reset() {
    yaw      = 0.0f;
    pitch    = 0.4f;
    distance = 15.0f;
    target   = { 0.0f, 0.0f, 0.0f };
}

Vector3 EditorCamera::position() const {
    auto p = MeshCraft::cameraOrbitPositionAlg(yaw, pitch, distance, {target.X, target.Y, target.Z});
    return { p[0], p[1], p[2] };
}

Matrix EditorCamera::viewMatrix() const {
    Vector3 pos = position();
    Vector3 up  = { 0.0f, 1.0f, 0.0f };
    // Avoid gimbal at straight up/down
    if (std::abs(pitch) > 1.47f) up = { std::sin(yaw), 0.0f, std::cos(yaw) };
    return Matrix::CreateLookAt(pos, target, up);
}

Matrix EditorCamera::projectionMatrix(float aspectRatio) const {
    if (orthographic) {
        float halfH = distance * std::tan(fovDegrees * (3.14159265f / 180.0f) * 0.5f);
        float halfW = halfH * aspectRatio;
        return Matrix::CreateOrthographic(halfW * 2.0f, halfH * 2.0f, nearPlane, farPlane);
    }
    float fovRad = fovDegrees * (3.14159265f / 180.0f);
    return Matrix::CreatePerspectiveFieldOfView(fovRad, aspectRatio, nearPlane, farPlane);
}

Vector3 EditorCamera::screenRayDirection(float ndcX, float ndcY, float aspectRatio) const {
    // Unproject NDC point at near plane into world space ray direction
    float fovRad = fovDegrees * (3.14159265f / 180.0f);
    float tanHalfFov = std::tan(fovRad * 0.5f);

    float localX =  ndcX * aspectRatio * tanHalfFov;
    float localY = -ndcY * tanHalfFov;
    float localZ = -1.0f;

    // Camera axes
    float cosP = std::cos(pitch), sinP = std::sin(pitch);
    float cosY = std::cos(yaw),   sinY = std::sin(yaw);

    Vector3 forward{  cosP * sinY,  sinP,  cosP * cosY };
    Vector3 right  {  cosY,         0.0f, -sinY        };
    Vector3 up     {  sinP * sinY, -cosP,  sinP * cosY };

    Vector3 dir{
        right.X * localX + up.X * localY + forward.X * localZ,
        right.Y * localX + up.Y * localY + forward.Y * localZ,
        right.Z * localX + up.Z * localY + forward.Z * localZ,
    };
    dir.Normalize();
    return dir;
}

} // namespace MeshCraft::Editor
