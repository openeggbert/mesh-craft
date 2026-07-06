#pragma once
// Pure CSG cache-invalidation hash — no CNA / ImGui / SDL / OpenGL dependencies.
// Extracted from SceneRenderer.cpp (STAB-0214/0215) so the content-hash logic
// behind SceneRenderer's CSG preview cache (K1) can be unit tested directly,
// without a live GraphicsDevice. Included by SceneRenderer.cpp and
// mc3_commands_test's editor_commands_test.cpp.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <cstddef>
#include <functional>
#include <string>

namespace MeshCraft {

inline std::size_t csgHashMixAlg(std::size_t h, std::size_t v) noexcept {
    return h ^ (v + 0x9e3779b9u + (h << 6) + (h >> 2));
}

// Recursively hash a CSG subtree's content: transforms, primitive params,
// children. Changing ANY input (move, resize, add/remove child) produces a
// different hash — this is exactly what SceneRenderer's content-hash CSG
// preview cache keys on to decide whether to re-evaluate Manifold.
inline std::size_t csgSubtreeHashAlg(const Mc3::Mc3Object& obj, const Mc3::Mc3Document& doc, int depth = 0) {
    if (depth > 12) return 0;
    std::size_t h = std::hash<std::string>{}(obj.id);
    auto hf = [&](float v) { h = csgHashMixAlg(h, std::hash<float>{}(v)); };
    auto hi = [&](int   v) { h = csgHashMixAlg(h, std::hash<int>{}(v));   };
    hi(static_cast<int>(obj.type));
    hi(obj.visible  ? 1 : 0);
    hi(obj.isCutter ? 1 : 0);
    const auto& t = obj.transform;
    hf(t.position[0]); hf(t.position[1]); hf(t.position[2]);
    hf(t.rotation[0]); hf(t.rotation[1]); hf(t.rotation[2]);
    hf(t.scale[0]);    hf(t.scale[1]);    hf(t.scale[2]);
    hf(t.pivot[0]);    hf(t.pivot[1]);    hf(t.pivot[2]);
    if (obj.primitive) {
        const auto& p = *obj.primitive;
        hi(static_cast<int>(p.primitiveType));
        hf(p.size[0]);       hf(p.size[1]);       hf(p.size[2]);
        hf(p.radius);        hf(p.height);
        hi(p.segments);
        hf(p.majorRadius);   hf(p.minorRadius);
    }
    if (obj.deform) {
        hf(obj.deform->scale[0]); hf(obj.deform->scale[1]); hf(obj.deform->scale[2]);
    }
    for (const auto& child : obj.children)
        h = csgHashMixAlg(h, csgSubtreeHashAlg(*child, doc, depth + 1));
    if (obj.type == Mc3::ObjectType::Instance) {
        const std::string& defKey = obj.resolvedInstanceDefinitionKey();
        auto it = doc.definitions.find(defKey);
        if (it != doc.definitions.end() && it->second)
            h = csgHashMixAlg(h, csgSubtreeHashAlg(*it->second, doc, depth + 1));
    }
    return h;
}

} // namespace MeshCraft
