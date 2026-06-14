#pragma once

#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <numbers>

namespace MeshCraft::Editor {

class EditorCamera {
public:
    EditorCamera();

    // Orbit / pan / zoom
    void orbit(float deltaYaw, float deltaPitch);
    void pan(float deltaX, float deltaY);
    void zoom(float delta);
    void focusOn(float x, float y, float z, float radius = 1.0f);
    void reset();

    [[nodiscard]] Microsoft::Xna::Framework::Matrix viewMatrix() const;
    [[nodiscard]] Microsoft::Xna::Framework::Matrix projectionMatrix(float aspectRatio) const;
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 position() const;

    // Ray-cast helpers for picking (returns direction in world space)
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 screenRayDirection(
        float ndcX, float ndcY, float aspectRatio) const;

    float yaw{0.0f};          // radians, horizontal orbit
    float pitch{0.4f};        // radians, vertical orbit (clamped)
    float distance{15.0f};    // distance from target
    float fovDegrees{60.0f};
    float nearPlane{0.1f};
    float farPlane{2000.0f};
    bool  orthographic{false};

    Microsoft::Xna::Framework::Vector3 target{0.0f, 0.0f, 0.0f};

private:
    static constexpr float kPitchMin = -1.48f;  // ~-85 deg
    static constexpr float kPitchMax =  1.48f;
    static constexpr float kDistMin  = 0.5f;
    static constexpr float kDistMax  = 2000.0f;
};

} // namespace MeshCraft::Editor
