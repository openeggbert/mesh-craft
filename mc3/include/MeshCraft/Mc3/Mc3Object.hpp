#pragma once

#include "MeshCraft/Mc3/Mc3CsgOperation.hpp"
#include "MeshCraft/Mc3/Mc3Deform.hpp"
#include "MeshCraft/Mc3/Mc3Extrude.hpp"
#include "MeshCraft/Mc3/Mc3Primitive.hpp"
#include "MeshCraft/Mc3/Mc3Transform.hpp"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

enum class UvProjection { Planar, Box, Sphere };


struct Mc3UvMapping {
    UvProjection projection{UvProjection::Planar};
    float scaleU{1.0f};
    float scaleV{1.0f};
    float offsetU{0.0f};
    float offsetV{0.0f};
    float rotation{0.0f}; // degrees
};

struct Mc3ObjectState {
    std::optional<std::array<float,3>> position;
    std::optional<std::array<float,3>> rotation;
    std::optional<std::array<float,3>> scale;
    std::optional<bool>                visible;
    std::optional<std::string>         material;
};

enum class ObjectType {
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
    Mesh,
    Extrude,
    Group,
    Instance,
    Union,
    Difference,
    Intersection,
    Area,
};

class Mc3Object {
public:
    ObjectType type{ObjectType::Box};

    std::string name;
    std::string id;

    Mc3Transform transform;
    std::optional<Mc3Deform> deform;

    std::string material;
    bool visible{true};
    std::string collision{"none"};
    std::string layer;   // named layer — empty = default layer
    std::vector<std::string> tags;

    std::optional<Mc3Primitive> primitive;
    std::optional<Mc3CsgOperation> csgOperation;
    std::optional<Mc3Extrude> extrude;

    std::string definition;                       // type == Instance (primary / fallback)
    std::vector<std::string> variantDefinitions;  // type == Instance: random pool of definitions
    std::string meshSource;      // type == Mesh
    std::string materialOverride;

    bool isCutter{false};        // role: cutter — CSG subtraction volume

    std::vector<std::shared_ptr<Mc3Object>> children;

    std::map<std::string, Mc3ObjectState> states;

    std::optional<Mc3UvMapping> uvMapping;

    // Opaque key/value pass-through store (mirrors <metadata> in the XSD).
    std::map<std::string, std::string> metadata;

    // --- Static factory methods -------------------------------------------
    // Each factory returns a shared_ptr so objects can be added directly to
    // children vectors or Mc3Document::objects.

    static std::shared_ptr<Mc3Object> makeBox(
        std::string name,
        std::array<float,3> size = {1.f,1.f,1.f},
        std::string material = "");

    static std::shared_ptr<Mc3Object> makeCube(
        std::string name, float side = 1.f, std::string material = "");

    static std::shared_ptr<Mc3Object> makeSphere(
        std::string name, float radius = 0.5f, int segments = 32,
        std::string material = "");

    static std::shared_ptr<Mc3Object> makeIcoSphere(
        std::string name, float radius = 0.5f, int subdivisions = 2,
        std::string material = "");

    static std::shared_ptr<Mc3Object> makeCylinder(
        std::string name, float radius = 0.5f, float height = 1.f,
        int segments = 32, std::string material = "");

    static std::shared_ptr<Mc3Object> makeCone(
        std::string name, float radius = 0.5f, float height = 1.f,
        int segments = 32, std::string material = "");

    static std::shared_ptr<Mc3Object> makePlane(
        std::string name, float width = 1.f, float depth = 1.f,
        std::string material = "");

    static std::shared_ptr<Mc3Object> makeTorus(
        std::string name, float majorRadius = 0.35f, float minorRadius = 0.15f,
        int segments = 32, std::string material = "");

    static std::shared_ptr<Mc3Object> makeCapsule(
        std::string name, float radius = 0.5f, float height = 1.f,
        int segments = 32, std::string material = "");

    static std::shared_ptr<Mc3Object> makeDisk(
        std::string name, float radius = 0.5f, int segments = 32,
        std::string material = "");

    static std::shared_ptr<Mc3Object> makeGrid(
        std::string name, int subdivisionsX = 4, int subdivisionsZ = 4,
        float width = 1.f, float depth = 1.f, std::string material = "");

    static std::shared_ptr<Mc3Object> makeGroup(
        std::string name,
        std::vector<std::shared_ptr<Mc3Object>> children = {});

    static std::shared_ptr<Mc3Object> makeInstance(
        std::string name, std::string definition, std::string material = "");

    // makeInstance with a pool of variant definitions (random selection)
    static std::shared_ptr<Mc3Object> makeInstanceVariant(
        std::string name, std::vector<std::string> definitions);

    static std::shared_ptr<Mc3Object> makeMesh(
        std::string name, std::string src, std::string material = "");

    // CSG operations — add children with isCutter=true for subtracted volumes
    static std::shared_ptr<Mc3Object> makeUnion(
        std::string name, std::vector<std::shared_ptr<Mc3Object>> children = {});
    static std::shared_ptr<Mc3Object> makeDifference(
        std::string name, std::vector<std::shared_ptr<Mc3Object>> children = {});
    static std::shared_ptr<Mc3Object> makeIntersection(
        std::string name, std::vector<std::shared_ptr<Mc3Object>> children = {});

    // --- Fluent setters ---------------------------------------------------
    // Return *this so callers can chain: obj->at(0,1,0)->withMaterial("stone")

    Mc3Object& at(float x, float y, float z);
    Mc3Object& rotatedBy(float rx, float ry, float rz);
    Mc3Object& scaledBy(float sx, float sy, float sz);
    Mc3Object& scaledBy(float s);
    Mc3Object& withId(std::string id);
    Mc3Object& withMaterial(std::string mat);
    Mc3Object& withVisible(bool v);
    Mc3Object& withCollision(std::string c);
    Mc3Object& withLayer(std::string l);
    Mc3Object& withTag(std::string tag);
    Mc3Object& withDeform(float sx, float sy, float sz);
    Mc3Object& addChild(std::shared_ptr<Mc3Object> child);
    Mc3Object& asCutter(bool v = true);
    Mc3Object& withUvMapping(Mc3UvMapping uv);
    Mc3Object& withUvMapping(UvProjection proj, float scaleU = 1.f, float scaleV = 1.f);
    Mc3Object& withMetadata(std::string key, std::string value);
};

} // namespace MeshCraft::Mc3
