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
    Mesh,
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

    std::string material;
    bool visible{true};
    std::string collision{"none"};
    std::vector<std::string> tags;

    std::optional<Mc3Primitive> primitive;
    std::optional<Mc3CsgOperation> csgOperation;

    std::string definition;      // type == Instance
    std::string meshSource;      // type == Mesh
    std::string materialOverride;

    // role: cutter — marks this as a CSG subtraction volume
    bool isCutter{false};

    std::vector<std::shared_ptr<Mc3Object>> children;

    // TODO: states, actions, uv mapping
};

} // namespace MeshCraft::Mc3
