#pragma once
#include <array>
#include <optional>
#include <string>

namespace MeshCraft::Mc3 {

enum class CameraType { Perspective, Orthographic };

struct Mc3Camera {
    std::string name;
    CameraType  type{CameraType::Perspective};

    std::array<float, 3> position{0.0f, 5.0f, 10.0f};
    std::array<float, 3> target{0.0f, 0.0f, 0.0f};
    std::optional<std::array<float, 3>> rotation; // alternative to target

    float nearPlane{0.1f};
    float farPlane{1000.0f};
    float fov{60.0f};        // perspective: vertical field of view in degrees
    float orthoSize{10.0f};  // orthographic: half-height in world units

    // --- Static factory helpers -------------------------------------------
    static Mc3Camera perspective(std::string name,
                                 std::array<float,3> pos    = {0.f,5.f,10.f},
                                 std::array<float,3> target = {0.f,0.f,0.f},
                                 float fov = 60.f,
                                 float nearP = 0.1f, float farP = 1000.f) {
        Mc3Camera c; c.name = std::move(name); c.type = CameraType::Perspective;
        c.position = pos; c.target = target;
        c.fov = fov; c.nearPlane = nearP; c.farPlane = farP; return c;
    }
    static Mc3Camera orthographic(std::string name,
                                  std::array<float,3> pos    = {0.f,5.f,10.f},
                                  std::array<float,3> target = {0.f,0.f,0.f},
                                  float orthoSize = 10.f,
                                  float nearP = 0.1f, float farP = 1000.f) {
        Mc3Camera c; c.name = std::move(name); c.type = CameraType::Orthographic;
        c.position = pos; c.target = target;
        c.orthoSize = orthoSize; c.nearPlane = nearP; c.farPlane = farP; return c;
    }

    Mc3Camera& withClip(float nearP, float farP) { nearPlane=nearP; farPlane=farP; return *this; }
    Mc3Camera& withFov(float f)       { fov = f;       return *this; }
    Mc3Camera& withOrthoSize(float s) { orthoSize = s; return *this; }
};

} // namespace MeshCraft::Mc3
