#pragma once

#include "MeshCraft/Mc3/Mc3CsgOperation.hpp"
#include "MeshCraft/Mc3/Mc3Primitive.hpp"
#include "MeshCraft/Mc3/Mc3Transform.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

enum class ObjectType {
    Box,
    Cube,
    Sphere,
    Cylinder,
    Cone,
    Plane,
    Mesh,       // external mesh reference
    Group,
    Instance,   // reference to a definition
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

    std::string material;
    bool visible{true};
    std::string collision{"none"};
    std::vector<std::string> tags;

    // Set when type is a primitive shape.
    std::optional<Mc3Primitive> primitive;

    // Set when type is a CSG operation.
    std::optional<Mc3CsgOperation> csgOperation;

    // Set when type == Instance.
    std::string definition;

    // Set when type == Mesh.
    std::string meshSource;
    std::string materialOverride;

    // role: cutter marks this object as a CSG subtraction volume.
    bool isCutter{false};

    // Child objects (used by Group, CSG operations).
    std::vector<std::shared_ptr<Mc3Object>> children;

    // TODO: states, actions, uv mapping
};

} // namespace MeshCraft::Mc3
