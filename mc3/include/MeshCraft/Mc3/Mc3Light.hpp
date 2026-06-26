#pragma once
#include <array>
#include <string>

namespace MeshCraft::Mc3 {

enum class LightType { Ambient, Directional, Spot, Point };

struct Mc3Light {
    LightType type{LightType::Directional};
    std::string name;

    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
    float brightness{1.0f};

    // Directional / Spot
    std::array<float, 3> direction{0.0f, -1.0f, 0.0f};

    // Spot / Point
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    float range{0.0f}; // 0 = unlimited

    // Spot only
    float angle{45.0f};   // half-angle in degrees
    float falloff{0.0f};  // 0..1

    bool castShadows{false};

    // --- Static factory helpers -------------------------------------------
    static Mc3Light directional(std::string name,
                                std::array<float,3> dir       = {0.f,-1.f,0.f},
                                std::array<float,3> color     = {1.f,1.f,1.f},
                                float brightness = 1.f) {
        Mc3Light l; l.type = LightType::Directional; l.name = std::move(name);
        l.direction = dir; l.color = color; l.brightness = brightness; return l;
    }
    static Mc3Light point(std::string name,
                          std::array<float,3> pos,
                          std::array<float,3> color = {1.f,1.f,1.f},
                          float brightness = 1.f, float range = 10.f) {
        Mc3Light l; l.type = LightType::Point; l.name = std::move(name);
        l.position = pos; l.color = color; l.brightness = brightness; l.range = range; return l;
    }
    static Mc3Light spot(std::string name,
                         std::array<float,3> pos,
                         std::array<float,3> dir    = {0.f,-1.f,0.f},
                         float angle = 45.f,
                         std::array<float,3> color  = {1.f,1.f,1.f},
                         float brightness = 1.f) {
        Mc3Light l; l.type = LightType::Spot; l.name = std::move(name);
        l.position = pos; l.direction = dir; l.angle = angle;
        l.color = color; l.brightness = brightness; return l;
    }
    static Mc3Light ambient(std::string name,
                            std::array<float,3> color = {0.1f,0.1f,0.1f},
                            float brightness = 1.f) {
        Mc3Light l; l.type = LightType::Ambient; l.name = std::move(name);
        l.color = color; l.brightness = brightness; return l;
    }

    Mc3Light& withShadows(bool v = true) { castShadows = v; return *this; }
    Mc3Light& withRange(float r)         { range = r;       return *this; }
    Mc3Light& withFalloff(float f)       { falloff = f;     return *this; }
};

} // namespace MeshCraft::Mc3
