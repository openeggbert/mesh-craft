#pragma once

#include <array>
#include <string>

namespace MeshCraft::Mc3 {

enum class PrimitiveType {
    Box,
    Cube,
    Sphere,
    Cylinder,
    Cone,
    Plane,
    Torus,
};

// Primitive shape parameters.
// Which fields are meaningful depends on the PrimitiveType.
struct Mc3Primitive {
    PrimitiveType primitiveType{PrimitiveType::Box};

    std::array<float, 3> size{1.0f, 1.0f, 1.0f}; // box/cube: w,h,d; plane: w,d
    float radius{0.5f};                            // sphere/cylinder/cone
    float height{1.0f};                            // cylinder/cone
    int segments{32};                              // sphere/cylinder/cone/torus
    std::string axis{"y"};                         // cylinder/plane: mesh-generation axis hint
    float majorRadius{0.35f};                      // torus: ring radius
    float minorRadius{0.15f};                      // torus: tube radius
};

} // namespace MeshCraft::Mc3
