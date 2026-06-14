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

    // TODO: actions, uv mapping
};

} // namespace MeshCraft::Mc3
