#pragma once

#include <array>

namespace MeshCraft::Mc3 {

// Local transform of an MC3 object. Rotation values use the owning
// Mc3Document's rotationUnits and eulerOrder (degrees/XYZ by default).
struct Mc3Transform {
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 3> rotation{0.0f, 0.0f, 0.0f}; // X/Y/Z angle components
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
    std::array<float, 3> pivot{0.0f, 0.0f, 0.0f};

    // --- Builder helpers --------------------------------------------------
    static Mc3Transform at(float x, float y, float z) {
        Mc3Transform t; t.position = {x, y, z}; return t;
    }
    static Mc3Transform atRotated(float x, float y, float z, float rx, float ry, float rz) {
        Mc3Transform t; t.position = {x,y,z}; t.rotation = {rx,ry,rz}; return t;
    }

    Mc3Transform& withPosition(float x, float y, float z) { position = {x,y,z}; return *this; }
    Mc3Transform& withRotation(float x, float y, float z) { rotation = {x,y,z}; return *this; }
    Mc3Transform& withScale(float x, float y, float z)    { scale = {x,y,z};    return *this; }
    Mc3Transform& withUniformScale(float s)                { scale = {s,s,s};    return *this; }
    Mc3Transform& withPivot(float x, float y, float z)    { pivot = {x,y,z};    return *this; }
};

} // namespace MeshCraft::Mc3
