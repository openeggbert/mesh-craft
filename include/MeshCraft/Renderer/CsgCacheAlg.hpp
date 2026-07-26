#pragma once
// Pure CSG cache-invalidation hash — no CNA / ImGui / SDL / OpenGL dependencies.
// Extracted from SceneRenderer.cpp (STAB-0214/0215) so the content-hash logic
// behind SceneRenderer's CSG preview cache (K1) can be unit tested directly,
// without a live GraphicsDevice. Included by SceneRenderer.cpp and
// mc3_commands_test's editor_commands_test.cpp.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Extrude.hpp>

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
    // SYS-W14-34: material relations do not alter Manifold topology, but do
    // alter the cached preview's material-index ranges. Include both fields
    // (rather than only the effective value) so changing precedence itself
    // cannot leave a stale child-material composition on screen.
    h = csgHashMixAlg(h, std::hash<std::string>{}(obj.material));
    h = csgHashMixAlg(h, std::hash<std::string>{}(obj.materialOverride));
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
    // CSG results now generate TEXCOORD_0 from the root's UV projection, so
    // changing any mapping parameter must rebuild the cached textured vertex
    // buffer even though the boolean topology itself is unchanged.
    if (obj.uvMapping) {
        hi(static_cast<int>(obj.uvMapping->projection));
        hf(obj.uvMapping->scaleU);   hf(obj.uvMapping->scaleV);
        hf(obj.uvMapping->offsetU);  hf(obj.uvMapping->offsetV);
        hf(obj.uvMapping->rotation);
    } else {
        hi(-1);
    }
    // csgOperation: a Union/Difference/Intersection node's own operation type
    // must be part of its hash — otherwise switching an existing CSG group's
    // operation (with no geometry change) produces an identical hash and the
    // preview cache never invalidates (AUDIT-0020).
    hi(obj.csgOperation ? static_cast<int>(obj.csgOperation->csgType) : -1);
    // extrude: an Extrude object's geometry lives entirely in this struct,
    // not in `primitive` — must be hashed or editing an extrude profile
    // inside a CSG tree won't invalidate the cached preview (AUDIT-0021).
    if (obj.extrude) {
        const auto& e = *obj.extrude;
        const auto& cs = e.crossSection;
        hi(static_cast<int>(cs.type));
        hf(cs.width); hf(cs.height); hf(cs.radius); hf(cs.innerRadius);
        hi(cs.sides); hi(cs.segments);
        for (const auto& pt : cs.customPoints) { hf(pt.x); hf(pt.y); }
        const auto& p = e.path;
        hi(static_cast<int>(p.type));
        hf(p.length); h = csgHashMixAlg(h, std::hash<std::string>{}(p.axis));
        hf(p.arcRadius); hf(p.arcAngle);
        hf(p.helixRadius); hf(p.helixHeight); hf(p.helixTurns);
        for (const auto& pp : p.points) {
            hf(pp.position[0]); hf(pp.position[1]); hf(pp.position[2]);
            hf(pp.controlIn[0]); hf(pp.controlIn[1]); hf(pp.controlIn[2]);
        }
        hf(e.twist); hi(e.segments); hi(e.smooth ? 1 : 0); hi(e.caps ? 1 : 0);
    }
    // meshSource: which mesh a Mesh-type object references — swapping the
    // referenced mesh on a CSG operand must invalidate the cache (AUDIT-0021).
    if (!obj.meshSource.empty())
        h = csgHashMixAlg(h, std::hash<std::string>{}(obj.meshSource));
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
