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
    Capsule,
    Disk,
    Grid,
    IcoSphere,
};

// Primitive shape parameters.
// Which fields are meaningful depends on the PrimitiveType.
struct Mc3Primitive {
    PrimitiveType primitiveType{PrimitiveType::Box};

    std::array<float, 3> size{1.0f, 1.0f, 1.0f}; // box/cube: w,h,d; plane: w,d
    float radius{0.5f};                            // sphere/cylinder/cone
    float height{1.0f};                            // cylinder/cone
    int segments{32};                              // sphere/cylinder/cone/torus
    std::string axis{"y"};                         // cylinder/plane/capsule/disk: mesh-generation axis hint
    float majorRadius{0.35f};                      // torus: ring radius
    float minorRadius{0.15f};                      // torus: tube radius
    int subdivisionsX{4};                          // grid: columns
    int subdivisionsZ{4};                          // grid: rows

    // --- Static factory helpers -------------------------------------------
    static Mc3Primitive box(std::array<float,3> s = {1.f,1.f,1.f}) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Box; p.size = s; return p;
    }
    static Mc3Primitive cube(float side = 1.f) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Cube; p.size = {side,side,side}; return p;
    }
    static Mc3Primitive sphere(float r = 0.5f, int segs = 32) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Sphere;
        p.radius = r; p.segments = segs; return p;
    }
    static Mc3Primitive cylinder(float r = 0.5f, float h = 1.f, int segs = 32) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Cylinder;
        p.radius = r; p.height = h; p.segments = segs; return p;
    }
    static Mc3Primitive cone(float r = 0.5f, float h = 1.f, int segs = 32) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Cone;
        p.radius = r; p.height = h; p.segments = segs; return p;
    }
    static Mc3Primitive plane(float w = 1.f, float d = 1.f) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Plane; p.size = {w,0.f,d}; return p;
    }
    static Mc3Primitive torus(float major = 0.35f, float minor = 0.15f, int segs = 32) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Torus;
        p.majorRadius = major; p.minorRadius = minor; p.segments = segs; return p;
    }
    static Mc3Primitive capsule(float r = 0.5f, float h = 1.f, int segs = 32) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Capsule;
        p.radius = r; p.height = h; p.segments = segs; return p;
    }
    static Mc3Primitive disk(float r = 0.5f, int segs = 32) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Disk;
        p.radius = r; p.segments = segs; return p;
    }
    static Mc3Primitive grid(int subdX = 4, int subdZ = 4, float w = 1.f, float d = 1.f) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::Grid;
        p.subdivisionsX = subdX; p.subdivisionsZ = subdZ; p.size = {w,0.f,d}; return p;
    }
    static Mc3Primitive icoSphere(float r = 0.5f, int segs = 2) {
        Mc3Primitive p; p.primitiveType = PrimitiveType::IcoSphere;
        p.radius = r; p.segments = segs; return p;
    }
};

} // namespace MeshCraft::Mc3
