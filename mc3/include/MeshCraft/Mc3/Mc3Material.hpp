#pragma once

#include <array>
#include <string>

namespace MeshCraft::Mc3 {

struct Mc3Material {
    std::string name;

    std::array<float, 4> baseColor{0.8f, 0.8f, 0.8f, 1.0f};
    std::string baseColorTexture;
    std::string normalTexture;
    std::string emissiveTexture;

    // Combined metallic-roughness texture (ORM: R=occlusion, G=roughness, B=metallic)
    std::string metallicRoughnessTexture;
    std::string occlusionTexture;

    float roughness{0.5f};
    float metallic{0.0f};
    float normalScale{1.0f};        // multiplier for the normal map
    float occlusionStrength{1.0f};  // 0 = no occlusion, 1 = full

    std::array<float, 3> emissiveColor{0.0f, 0.0f, 0.0f};

    std::string alphaMode{"opaque"}; // opaque | mask | blend
    float alphaCutoff{0.5f};         // used when alphaMode == "mask"
    bool doubleSided{false};

    // --- Constructors -----------------------------------------------------
    Mc3Material() = default;

    // Opaque PBR material: name, base color, roughness, metallic
    Mc3Material(std::string n, std::array<float,4> color,
                float rough = 0.5f, float metal = 0.0f)
        : name(std::move(n)), baseColor(color), roughness(rough), metallic(metal) {}

    // --- Static factory helpers -------------------------------------------
    static Mc3Material opaque(std::string name, std::array<float,4> color,
                              float roughness = 0.5f, float metallic = 0.0f) {
        return Mc3Material(std::move(name), color, roughness, metallic);
    }
    static Mc3Material emissive(std::string name, std::array<float,3> emissiveColor,
                                std::array<float,4> baseColor = {0.f,0.f,0.f,1.f}) {
        Mc3Material m(std::move(name), baseColor);
        m.emissiveColor = emissiveColor;
        return m;
    }
    static Mc3Material textured(std::string name, std::string baseColorTex,
                                float roughness = 0.5f, float metallic = 0.0f) {
        Mc3Material m; m.name = std::move(name);
        m.baseColorTexture = std::move(baseColorTex);
        m.roughness = roughness; m.metallic = metallic;
        return m;
    }
    static Mc3Material metal(std::string name, std::array<float,4> color,
                             float roughness = 0.1f) {
        return Mc3Material(std::move(name), color, roughness, 1.0f);
    }
    static Mc3Material glass(std::string name, std::array<float,4> color = {1.f,1.f,1.f,0.2f}) {
        Mc3Material m(std::move(name), color, 0.0f, 0.0f);
        m.alphaMode = "blend";
        return m;
    }

    // --- Fluent setters ---------------------------------------------------
    Mc3Material& withEmissive(std::array<float,3> ec) { emissiveColor = ec; return *this; }
    Mc3Material& withEmissive(float r, float g, float b) { emissiveColor = {r,g,b}; return *this; }
    Mc3Material& withNormalMap(std::string tex, float scale = 1.0f) {
        normalTexture = std::move(tex); normalScale = scale; return *this;
    }
    Mc3Material& withRoughnessMetal(float r, float m) { roughness=r; metallic=m; return *this; }
    Mc3Material& withAlphaMode(std::string mode, float cutoff = 0.5f) {
        alphaMode = std::move(mode); alphaCutoff = cutoff; return *this;
    }
    Mc3Material& withDoubleSided(bool ds = true) { doubleSided = ds; return *this; }
};

} // namespace MeshCraft::Mc3
