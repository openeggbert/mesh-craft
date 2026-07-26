// SYS-W11-05 — shared random-document generator + deep comparator used by
// BOTH the MC3 XML random round-trip test (mc3/test/random_roundtrip_test.cpp)
// and the MCB binary random round-trip test
// (mcb/test/mcb_random_roundtrip_test.cpp).
//
// This is deliberately NOT under mc3/include/ (Mc3's public API) -- it is
// test-only infrastructure, header-only, included directly by the two .cpp
// files above via an explicit relative/-I path set up in the respective
// CMakeLists.txt. It does not change Mc3Document's public API.
//
// Purpose: existing round-trip coverage (mc3/test/roundtrip_test.cpp,
// mcb/test/mcb_roundtrip_test.cpp) is thorough but exercises FIXED, hand-
// picked scenarios one feature at a time. This generator instead builds a
// large number of RANDOM (seeded, reproducible) documents combining many
// features with random parameter values in a single pass, which is a
// different -- and complementary -- kind of bug-finding: it can turn up
// interaction/edge-value bugs that a fixed fixture wouldn't happen to hit
// (e.g. a specific float value that round-trips lossily, a rare enum
// combination, an empty-vs-populated optional field combined with a
// particular sibling field).
//
// Deliberately scoped: primitives (all PrimitiveType values), groups,
// simple CSG (difference of two primitives), materials, textures, lights
// (all LightType values), cameras (both CameraType values), one action with
// several channels/keyframes (all three Interpolation values), and
// meta/tags/deform. Extrude, Instance/definitions, SceneState, Trigger,
// Script, Sound/Music, SVG texture and embed are intentionally NOT included
// here -- those already have deep, dedicated fixed-fixture coverage in
// roundtrip_test.cpp/mcb_roundtrip_test.cpp, and their semantics (e.g.
// Instance's per-consumer definition resolution, Extrude's path/cross-section
// combinatorics) are subtle enough that inventing random combinations of them
// here would risk false-positive test failures from this generator's own
// misunderstanding rather than genuine parser/writer bugs.

#pragma once

#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3CsgOperation.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
#include "MeshCraft/Mc3/Mc3Camera.hpp"
#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Primitive.hpp"
#include "MeshCraft/Mc3/Mc3Texture.hpp"

#include <array>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace MeshCraftTest {

using namespace MeshCraft::Mc3;

// --- RNG helpers ------------------------------------------------------------

inline float randFloat(std::mt19937& rng, float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng);
}

inline int randInt(std::mt19937& rng, int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

inline bool randBool(std::mt19937& rng) { return randInt(rng, 0, 1) == 1; }

inline std::string randIdString(std::mt19937& rng, const std::string& prefix) {
    return prefix + std::to_string(randInt(rng, 0, 1'000'000));
}

inline std::array<float, 3> randVec3(std::mt19937& rng, float lo, float hi) {
    return {randFloat(rng, lo, hi), randFloat(rng, lo, hi), randFloat(rng, lo, hi)};
}

// --- Random leaf builders -----------------------------------------------

//
// IMPORTANT: built via the SAME static factory helpers Mc3Primitive itself
// exposes (Mc3Primitive::box/cube/sphere/...), not by populating every field
// on a default-constructed Mc3Primitive. Mc3XmlWriter.cpp only serializes
// the fields that are semantically meaningful for a given primitiveType
// (see its writeObject() switch on ObjectType -- e.g. Sphere writes only
// radius/segments, never `size`; Plane writes only size.x/size.z, never
// size.y), and Mc3XmlParser.cpp correspondingly leaves any non-serialized
// field at Mc3Primitive's struct default on load. A generator that set
// (say) `size` on a random Sphere would therefore see a "mismatch" after
// round-tripping that is NOT a real bug -- `size` was never part of a
// Sphere's persisted contract in the first place, matching the plain
// default-constructed Mc3Primitive{} every real Sphere-producing call site
// (Mc3Object::makeSphere, the factory below) already produces. Building
// through the real factories keeps every field this generator sets
// semantically meaningful for its type, so a genuine mismatch always means
// the writer/parser actually lost data, not that the test invented a
// nonsensical primitive.
inline Mc3Primitive randPrimitive(std::mt19937& rng) {
    switch (randInt(rng, 0, 10)) {
        case 0:
            return Mc3Primitive::box({randFloat(rng, 0.01f, 10.f), randFloat(rng, 0.01f, 10.f),
                                       randFloat(rng, 0.01f, 10.f)});
        case 1:
            return Mc3Primitive::cube(randFloat(rng, 0.01f, 10.f));
        case 2:
            return Mc3Primitive::sphere(randFloat(rng, 0.01f, 5.f), randInt(rng, 3, 64));
        case 3:
            return Mc3Primitive::cylinder(randFloat(rng, 0.01f, 5.f), randFloat(rng, 0.01f, 10.f),
                                           randInt(rng, 3, 64));
        case 4:
            return Mc3Primitive::cone(randFloat(rng, 0.01f, 5.f), randFloat(rng, 0.01f, 10.f),
                                       randInt(rng, 3, 64));
        case 5:
            return Mc3Primitive::plane(randFloat(rng, 0.01f, 10.f), randFloat(rng, 0.01f, 10.f));
        case 6:
            return Mc3Primitive::torus(randFloat(rng, 0.05f, 3.f), randFloat(rng, 0.01f, 1.f),
                                        randInt(rng, 3, 64));
        case 7:
            return Mc3Primitive::capsule(randFloat(rng, 0.01f, 5.f), randFloat(rng, 0.01f, 10.f),
                                          randInt(rng, 3, 64));
        case 8:
            // minorRadius (inner_radius) deliberately > 0 -- the writer only
            // emits inner_radius when > 0.0f (mirroring testDiskSolidRoundtrip's
            // "0.0 == solid, not written" contract in roundtrip_test.cpp);
            // a randomly-chosen 0 would still round-trip correctly (both
            // sides default to 0), but keeping this generator's disks always
            // "ring" disks exercises the more interesting attribute-present
            // path on every trial.
            return Mc3Primitive::disk(randFloat(rng, 0.01f, 5.f), randInt(rng, 3, 64));
        case 9:
            return Mc3Primitive::grid(randInt(rng, 1, 16), randInt(rng, 1, 16),
                                       randFloat(rng, 0.01f, 10.f), randFloat(rng, 0.01f, 10.f));
        default:
            return Mc3Primitive::icoSphere(randFloat(rng, 0.01f, 5.f), randInt(rng, 1, 4));
    }
}

inline Mc3Material randMaterial(std::mt19937& rng, const std::string& name) {
    Mc3Material m;
    m.name = name;
    m.baseColor = {randFloat(rng, 0, 1), randFloat(rng, 0, 1), randFloat(rng, 0, 1), randFloat(rng, 0, 1)};
    m.roughness = randFloat(rng, 0, 1);
    m.metallic = randFloat(rng, 0, 1);
    m.normalScale = randFloat(rng, 0, 2);
    m.occlusionStrength = randFloat(rng, 0, 1);
    m.emissiveColor = randVec3(rng, 0, 1);
    m.alphaMode = randBool(rng) ? "opaque" : (randBool(rng) ? "mask" : "blend");
    m.alphaCutoff = randFloat(rng, 0, 1);
    m.doubleSided = randBool(rng);
    return m;
}

inline Mc3Texture randTexture(std::mt19937& rng, const std::string& name) {
    Mc3Texture t;
    t.name = name;
    t.uri = name + ".png";
    t.wrapU = randBool(rng) ? "repeat" : "clamp";
    t.wrapV = randBool(rng) ? "repeat" : "clamp";
    t.filter = randBool(rng) ? "linear" : "nearest";
    t.colorSpace = randBool(rng) ? "srgb" : "linear";
    t.mipMaps = randBool(rng);
    return t;
}

inline Mc3Light randLight(std::mt19937& rng, const std::string& name) {
    static const LightType kTypes[] = {LightType::Ambient, LightType::Directional,
                                        LightType::Spot, LightType::Point};
    Mc3Light l;
    l.type = kTypes[static_cast<size_t>(randInt(rng, 0, 3))];
    l.name = name;
    l.color = randVec3(rng, 0, 1);
    l.brightness = randFloat(rng, 0, 5);
    l.direction = randVec3(rng, -1, 1);
    l.position = randVec3(rng, -20, 20);
    l.range = randFloat(rng, 0, 50);
    l.angle = randFloat(rng, 1, 89);
    l.falloff = randFloat(rng, 0, 1);
    // castShadows is only parsed (and therefore only round-trips) for
    // Directional/Spot/Point -- Mc3XmlParser.cpp's parseLights() ambient
    // branch never reads cast_shadows, so an Ambient light's castShadows
    // silently resets to the struct default (false) on reload. Leaving it
    // at that same default here for Ambient keeps this generator's output
    // within the field's actual persisted contract (compareDocuments below
    // does compare castShadows for every light, so this must match what a
    // round-trip can actually preserve).
    if (l.type != LightType::Ambient) l.castShadows = randBool(rng);
    return l;
}

inline Mc3Camera randCamera(std::mt19937& rng, const std::string& name) {
    Mc3Camera c;
    c.name = name;
    c.type = randBool(rng) ? CameraType::Perspective : CameraType::Orthographic;
    c.position = randVec3(rng, -50, 50);
    c.target = randVec3(rng, -50, 50);
    c.nearPlane = randFloat(rng, 0.01f, 1.f);
    c.farPlane = randFloat(rng, 10.f, 5000.f);
    c.fov = randFloat(rng, 10, 120);
    c.orthoSize = randFloat(rng, 1, 100);
    c.orthoAspect = randFloat(rng, 0.5f, 2.f);
    return c;
}

inline Mc3Transform randTransform(std::mt19937& rng) {
    Mc3Transform t;
    t.position = randVec3(rng, -100, 100);
    t.rotation = randVec3(rng, -180, 180);
    t.scale = randVec3(rng, 0.1f, 5.f);
    t.pivot = randVec3(rng, -5, 5);
    return t;
}

// Builds a single non-CSG "leaf" object: a random primitive with a random
// transform/material/tags/metadata/deform.
inline std::shared_ptr<Mc3Object> randLeafObject(std::mt19937& rng, const std::string& idPrefix,
                                                  const std::vector<std::string>& materialNames) {
    auto obj = std::make_shared<Mc3Object>();
    obj->id = randIdString(rng, idPrefix);
    obj->name = obj->id + "_name";
    obj->type = ObjectType::Box; // overwritten to match primitiveType below via factory-equivalent mapping
    Mc3Primitive prim = randPrimitive(rng);
    // ObjectType must mirror PrimitiveType for the primitive-typed branch --
    // Mc3XmlParser/McbReader key the shape on ObjectType, not PrimitiveType
    // (see Mc3Object::makeBox/makeSphere/... factories for the same mapping).
    switch (prim.primitiveType) {
        case PrimitiveType::Box:       obj->type = ObjectType::Box; break;
        case PrimitiveType::Cube:      obj->type = ObjectType::Cube; break;
        case PrimitiveType::Sphere:    obj->type = ObjectType::Sphere; break;
        case PrimitiveType::Cylinder:  obj->type = ObjectType::Cylinder; break;
        case PrimitiveType::Cone:      obj->type = ObjectType::Cone; break;
        case PrimitiveType::Plane:     obj->type = ObjectType::Plane; break;
        case PrimitiveType::Torus:     obj->type = ObjectType::Torus; break;
        case PrimitiveType::Capsule:   obj->type = ObjectType::Capsule; break;
        case PrimitiveType::Disk:      obj->type = ObjectType::Disk; break;
        case PrimitiveType::Grid:      obj->type = ObjectType::Grid; break;
        case PrimitiveType::IcoSphere: obj->type = ObjectType::IcoSphere; break;
    }
    obj->primitive = prim;
    obj->transform = randTransform(rng);
    if (!materialNames.empty() && randBool(rng))
        obj->material = materialNames[static_cast<size_t>(randInt(rng, 0, static_cast<int>(materialNames.size()) - 1))];
    obj->visible = randBool(rng);
    obj->collision = randBool(rng) ? "none" : "static";
    if (randBool(rng)) {
        int nTags = randInt(rng, 0, 3);
        for (int i = 0; i < nTags; ++i) obj->tags.push_back(randIdString(rng, "tag"));
    }
    if (randBool(rng)) obj->metadata[randIdString(rng, "k")] = randIdString(rng, "v");
    if (randBool(rng)) {
        Mc3Deform d;
        d.scale = randVec3(rng, 0.1f, 3.f);
        obj->deform = d;
    }
    return obj;
}

// Builds a small random object tree: a mix of leaf primitives, a Group with
// 0-3 leaf children, and (with some probability) a CSG Difference of two
// leaves (second marked as cutter). `count` roughly bounds the number of
// top-level entries produced.
inline std::vector<std::shared_ptr<Mc3Object>> randObjectTree(
    std::mt19937& rng, int count, const std::vector<std::string>& materialNames) {
    std::vector<std::shared_ptr<Mc3Object>> result;
    for (int i = 0; i < count; ++i) {
        int kind = randInt(rng, 0, 9);
        if (kind < 6) {
            result.push_back(randLeafObject(rng, "obj", materialNames));
        } else if (kind < 8) {
            auto group = std::make_shared<Mc3Object>();
            group->id = randIdString(rng, "grp");
            group->name = group->id + "_name";
            group->type = ObjectType::Group;
            group->transform = randTransform(rng);
            int nChildren = randInt(rng, 0, 3);
            for (int c = 0; c < nChildren; ++c)
                group->children.push_back(randLeafObject(rng, "child", materialNames));
            result.push_back(group);
        } else {
            auto csg = std::make_shared<Mc3Object>();
            csg->id = randIdString(rng, "csg");
            csg->name = csg->id + "_name";
            csg->type = ObjectType::Difference;
            Mc3CsgOperation op;
            op.csgType = CsgType::Difference;
            csg->csgOperation = op;
            csg->transform = randTransform(rng);
            auto base = randLeafObject(rng, "base", materialNames);
            auto cutter = randLeafObject(rng, "cutter", materialNames);
            cutter->isCutter = true;
            csg->children.push_back(base);
            csg->children.push_back(cutter);
            result.push_back(csg);
        }
    }
    return result;
}

inline Mc3Action randAction(std::mt19937& rng, const std::string& name,
                            const std::vector<std::string>& objectIds) {
    static const AnimatedProperty kProps[] = {
        AnimatedProperty::PositionX, AnimatedProperty::PositionY, AnimatedProperty::PositionZ,
        AnimatedProperty::RotationX, AnimatedProperty::ScaleX, AnimatedProperty::Visible,
        AnimatedProperty::MaterialRoughness, AnimatedProperty::MaterialMetallic,
    };
    Mc3Action action;
    action.name = name;
    action.duration = randFloat(rng, 0.1f, 10.f);
    action.loop = randBool(rng);
    action.autoplay = randBool(rng);
    action.timeScale = randFloat(rng, 0.25f, 4.f);

    int nChannels = randInt(rng, 1, 3);
    for (int i = 0; i < nChannels; ++i) {
        Mc3Channel ch;
        ch.targetObject = objectIds.empty() ? "none" :
            objectIds[static_cast<size_t>(randInt(rng, 0, static_cast<int>(objectIds.size()) - 1))];
        ch.property = kProps[static_cast<size_t>(randInt(rng, 0, 7))];
        int nKeys = randInt(rng, 2, 5);
        float t = 0.f;
        for (int k = 0; k < nKeys; ++k) {
            t += randFloat(rng, 0.05f, 1.5f);
            float v = randFloat(rng, -10, 10);
            int interp = randInt(rng, 0, 2);
            if (interp == 0)
                ch.keyframes.push_back(Mc3Keyframe::linear(t, v));
            else if (interp == 1)
                ch.keyframes.push_back(Mc3Keyframe::step(t, v));
            else
                ch.keyframes.push_back(Mc3Keyframe::bezier(
                    t, v,
                    Mc3BezierHandle{randFloat(rng, -0.3f, -0.01f), randFloat(rng, -1, 1)},
                    Mc3BezierHandle{randFloat(rng, 0.01f, 0.3f), randFloat(rng, -1, 1)}));
        }
        action.channels.push_back(std::move(ch));
    }
    return action;
}

// Collects every object id in the tree (including nested children), used to
// give randAction() valid-looking channel targets.
inline void collectObjectIds(const std::vector<std::shared_ptr<Mc3Object>>& objs,
                              std::vector<std::string>& out) {
    for (const auto& o : objs) {
        out.push_back(o->id);
        collectObjectIds(o->children, out);
    }
}

// Builds one full random document. `complexity` roughly controls how much
// content it contains (object count, material/texture/light/camera counts).
inline Mc3Document buildRandomDocument(std::mt19937& rng, int complexity) {
    Mc3Document doc;
    doc.model = randIdString(rng, "RandomModel");
    if (randBool(rng)) doc.meta[randIdString(rng, "mk")] = randIdString(rng, "mv");

    int nMaterials = randInt(rng, 0, complexity);
    std::vector<std::string> materialNames;
    for (int i = 0; i < nMaterials; ++i) {
        std::string mname = randIdString(rng, "mat");
        doc.materials[mname] = randMaterial(rng, mname);
        materialNames.push_back(mname);
    }

    int nTextures = randInt(rng, 0, std::min(3, complexity));
    for (int i = 0; i < nTextures; ++i) {
        std::string tname = randIdString(rng, "tex");
        doc.textures[tname] = randTexture(rng, tname);
    }

    int nLights = randInt(rng, 0, std::min(4, complexity));
    for (int i = 0; i < nLights; ++i)
        doc.lights.push_back(randLight(rng, randIdString(rng, "light")));

    int nCameras = randInt(rng, 0, std::min(3, complexity));
    for (int i = 0; i < nCameras; ++i)
        doc.cameras.push_back(randCamera(rng, randIdString(rng, "cam")));

    doc.objects = randObjectTree(rng, complexity, materialNames);

    if (randBool(rng) && !doc.objects.empty()) {
        std::vector<std::string> ids;
        collectObjectIds(doc.objects, ids);
        std::string actionName = randIdString(rng, "action");
        doc.actions[actionName] = randAction(rng, actionName, ids);
    }

    return doc;
}

// --- Deep comparison ---------------------------------------------------

inline bool nearlyEqual(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps * std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
}

inline bool vecNearlyEqual(const std::array<float,3>& a, const std::array<float,3>& b) {
    return nearlyEqual(a[0], b[0]) && nearlyEqual(a[1], b[1]) && nearlyEqual(a[2], b[2]);
}

// Recursively compares two object trees. Appends a human-readable mismatch
// description to `errors` for every field that differs; returns via `errors`
// being empty on success. Deliberately continues past the first mismatch so
// a single failing document produces a full diagnostic, not just the first
// difference found.
inline void compareObjects(const std::shared_ptr<Mc3Object>& a, const std::shared_ptr<Mc3Object>& b,
                            const std::string& path, std::vector<std::string>& errors) {
    if (!a || !b) {
        if (a != b) errors.push_back(path + ": null-ness mismatch");
        return;
    }
    if (a->id != b->id) errors.push_back(path + ": id '" + a->id + "' != '" + b->id + "'");
    if (a->type != b->type) errors.push_back(path + ": ObjectType mismatch for id " + a->id);
    if (!vecNearlyEqual(a->transform.position, b->transform.position))
        errors.push_back(path + ": transform.position mismatch for id " + a->id);
    if (!vecNearlyEqual(a->transform.rotation, b->transform.rotation))
        errors.push_back(path + ": transform.rotation mismatch for id " + a->id);
    if (!vecNearlyEqual(a->transform.scale, b->transform.scale))
        errors.push_back(path + ": transform.scale mismatch for id " + a->id);
    if (!vecNearlyEqual(a->transform.pivot, b->transform.pivot))
        errors.push_back(path + ": transform.pivot mismatch for id " + a->id);
    if (a->material != b->material)
        errors.push_back(path + ": material mismatch for id " + a->id);
    if (a->visible != b->visible)
        errors.push_back(path + ": visible mismatch for id " + a->id);
    if (a->isCutter != b->isCutter)
        errors.push_back(path + ": isCutter mismatch for id " + a->id);
    if (a->tags != b->tags)
        errors.push_back(path + ": tags mismatch for id " + a->id);
    if (a->primitive.has_value() != b->primitive.has_value()) {
        errors.push_back(path + ": primitive presence mismatch for id " + a->id);
    } else if (a->primitive.has_value()) {
        const auto& pa = *a->primitive;
        const auto& pb = *b->primitive;
        if (pa.primitiveType != pb.primitiveType)
            errors.push_back(path + ": primitiveType mismatch for id " + a->id);
        // Plane is a partial user of `size`: Mc3XmlWriter.cpp writes only
        // size.x/size.z for a plane (the width/depth of a flat quad) and
        // never touches size.y on either side of the codec -- the
        // Mc3Primitive::plane() factory sets size.y=0 purely as its own
        // in-memory convention, but Mc3XmlParser.cpp's plane branch leaves
        // size.y at Mc3Primitive's plain struct default (1.0f) since it
        // never reads/writes that slot at all. So size.y is not part of
        // Plane's persisted contract in either direction; only x/z round-trip.
        bool sizeMatches = (pa.primitiveType == PrimitiveType::Plane)
            ? (nearlyEqual(pa.size[0], pb.size[0]) && nearlyEqual(pa.size[2], pb.size[2]))
            : vecNearlyEqual(pa.size, pb.size);
        if (!sizeMatches)
            errors.push_back(path + ": primitive.size mismatch for id " + a->id);
        if (!nearlyEqual(pa.radius, pb.radius))
            errors.push_back(path + ": primitive.radius mismatch for id " + a->id);
        if (!nearlyEqual(pa.height, pb.height))
            errors.push_back(path + ": primitive.height mismatch for id " + a->id);
        if (pa.segments != pb.segments)
            errors.push_back(path + ": primitive.segments mismatch for id " + a->id);
    }
    if (a->csgOperation.has_value() != b->csgOperation.has_value()) {
        errors.push_back(path + ": csgOperation presence mismatch for id " + a->id);
    } else if (a->csgOperation.has_value() && a->csgOperation->csgType != b->csgOperation->csgType) {
        errors.push_back(path + ": csgOperation.csgType mismatch for id " + a->id);
    }
    if (a->deform.has_value() != b->deform.has_value()) {
        errors.push_back(path + ": deform presence mismatch for id " + a->id);
    } else if (a->deform.has_value() && !vecNearlyEqual(a->deform->scale, b->deform->scale)) {
        errors.push_back(path + ": deform.scale mismatch for id " + a->id);
    }
    if (a->children.size() != b->children.size()) {
        errors.push_back(path + ": children count mismatch for id " + a->id + " (" +
                          std::to_string(a->children.size()) + " vs " +
                          std::to_string(b->children.size()) + ")");
        return;
    }
    for (size_t i = 0; i < a->children.size(); ++i)
        compareObjects(a->children[i], b->children[i], path + "/" + a->id, errors);
}

// Compares two documents produced by round-tripping the SAME source document
// through some codec (XML text or MCB binary). Returns a list of mismatch
// descriptions (empty == documents are semantically equivalent).
inline std::vector<std::string> compareDocuments(const Mc3Document& a, const Mc3Document& b) {
    std::vector<std::string> errors;
    if (a.model != b.model) errors.push_back("model mismatch: '" + a.model + "' != '" + b.model + "'");
    if (a.objects.size() != b.objects.size()) {
        errors.push_back("top-level object count mismatch (" + std::to_string(a.objects.size()) +
                          " vs " + std::to_string(b.objects.size()) + ")");
    } else {
        for (size_t i = 0; i < a.objects.size(); ++i)
            compareObjects(a.objects[i], b.objects[i], "objects[" + std::to_string(i) + "]", errors);
    }
    if (a.materials.size() != b.materials.size())
        errors.push_back("materials count mismatch");
    for (const auto& [name, ma] : a.materials) {
        auto it = b.materials.find(name);
        if (it == b.materials.end()) { errors.push_back("material '" + name + "' missing after round-trip"); continue; }
        const auto& mb = it->second;
        if (!nearlyEqual(ma.baseColor[0], mb.baseColor[0]) || !nearlyEqual(ma.baseColor[1], mb.baseColor[1]) ||
            !nearlyEqual(ma.baseColor[2], mb.baseColor[2]) || !nearlyEqual(ma.baseColor[3], mb.baseColor[3]))
            errors.push_back("material '" + name + "' baseColor mismatch");
        if (!nearlyEqual(ma.roughness, mb.roughness)) errors.push_back("material '" + name + "' roughness mismatch");
        if (!nearlyEqual(ma.metallic, mb.metallic)) errors.push_back("material '" + name + "' metallic mismatch");
        if (ma.doubleSided != mb.doubleSided) errors.push_back("material '" + name + "' doubleSided mismatch");
    }
    if (a.textures.size() != b.textures.size())
        errors.push_back("textures count mismatch");
    for (const auto& [name, ta] : a.textures) {
        auto it = b.textures.find(name);
        if (it == b.textures.end()) { errors.push_back("texture '" + name + "' missing after round-trip"); continue; }
        if (ta.uri != it->second.uri) errors.push_back("texture '" + name + "' uri mismatch");
        if (ta.wrapU != it->second.wrapU) errors.push_back("texture '" + name + "' wrapU mismatch");
        if (ta.mipMaps != it->second.mipMaps) errors.push_back("texture '" + name + "' mipMaps mismatch");
    }
    if (a.lights.size() != b.lights.size()) {
        errors.push_back("lights count mismatch");
    } else {
        for (size_t i = 0; i < a.lights.size(); ++i) {
            const auto& la = a.lights[i];
            const auto& lb = b.lights[i];
            if (la.type != lb.type) errors.push_back("light[" + std::to_string(i) + "] type mismatch");
            if (!vecNearlyEqual(la.color, lb.color)) errors.push_back("light[" + std::to_string(i) + "] color mismatch");
            if (!nearlyEqual(la.brightness, lb.brightness)) errors.push_back("light[" + std::to_string(i) + "] brightness mismatch");
            if (la.castShadows != lb.castShadows) errors.push_back("light[" + std::to_string(i) + "] castShadows mismatch");
        }
    }
    if (a.cameras.size() != b.cameras.size()) {
        errors.push_back("cameras count mismatch");
    } else {
        for (size_t i = 0; i < a.cameras.size(); ++i) {
            const auto& ca = a.cameras[i];
            const auto& cb = b.cameras[i];
            if (ca.type != cb.type) errors.push_back("camera[" + std::to_string(i) + "] type mismatch");
            if (!vecNearlyEqual(ca.position, cb.position)) errors.push_back("camera[" + std::to_string(i) + "] position mismatch");
            if (!nearlyEqual(ca.fov, cb.fov)) errors.push_back("camera[" + std::to_string(i) + "] fov mismatch");
        }
    }
    if (a.actions.size() != b.actions.size())
        errors.push_back("actions count mismatch");
    for (const auto& [name, actA] : a.actions) {
        auto it = b.actions.find(name);
        if (it == b.actions.end()) { errors.push_back("action '" + name + "' missing after round-trip"); continue; }
        const auto& actB = it->second;
        if (!nearlyEqual(actA.duration, actB.duration)) errors.push_back("action '" + name + "' duration mismatch");
        if (actA.loop != actB.loop) errors.push_back("action '" + name + "' loop mismatch");
        if (actA.channels.size() != actB.channels.size()) {
            errors.push_back("action '" + name + "' channel count mismatch");
            continue;
        }
        for (size_t c = 0; c < actA.channels.size(); ++c) {
            const auto& cha = actA.channels[c];
            const auto& chb = actB.channels[c];
            if (cha.targetObject != chb.targetObject)
                errors.push_back("action '" + name + "' channel[" + std::to_string(c) + "] targetObject mismatch");
            if (cha.property != chb.property)
                errors.push_back("action '" + name + "' channel[" + std::to_string(c) + "] property mismatch");
            if (cha.keyframes.size() != chb.keyframes.size()) {
                errors.push_back("action '" + name + "' channel[" + std::to_string(c) + "] keyframe count mismatch");
                continue;
            }
            for (size_t k = 0; k < cha.keyframes.size(); ++k) {
                if (!nearlyEqual(cha.keyframes[k].time, chb.keyframes[k].time) ||
                    !nearlyEqual(cha.keyframes[k].value, chb.keyframes[k].value) ||
                    cha.keyframes[k].interpolation != chb.keyframes[k].interpolation) {
                    errors.push_back("action '" + name + "' channel[" + std::to_string(c) +
                                      "] keyframe[" + std::to_string(k) + "] mismatch");
                }
            }
        }
    }
    if (a.eventBindings.size() != b.eventBindings.size()) {
        errors.push_back("event bindings count mismatch");
    } else {
        for (size_t i = 0; i < a.eventBindings.size(); ++i) {
            const auto& ea = a.eventBindings[i];
            const auto& eb = b.eventBindings[i];
            if (ea.id != eb.id || ea.sourceObjectId != eb.sourceObjectId || ea.event != eb.event ||
                ea.targetType != eb.targetType || ea.targetId != eb.targetId || ea.enabled != eb.enabled ||
                ea.once != eb.once || !nearlyEqual(ea.cooldown, eb.cooldown) ||
                !nearlyEqual(ea.interval, eb.interval))
                errors.push_back("event binding[" + std::to_string(i) + "] mismatch");
        }
    }
    return errors;
}

} // namespace MeshCraftTest
