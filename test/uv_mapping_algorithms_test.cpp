// SYS-W14-32 -- viewport UV semantics against the exporter's independent mesh path.

#include <MeshCraft/UvMappingAlgorithms.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>
#include "MeshBuilder.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) std::cout << "PASS: " << message << "\n";
    else { std::cerr << "FAIL: " << message << "\n"; ++failures; }
}

std::vector<UvPositionAlg> positionsOf(const mc3togltf::MeshData& mesh) {
    std::vector<UvPositionAlg> result;
    result.reserve(mesh.positions.size() / 3);
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3)
        result.push_back({mesh.positions[i], mesh.positions[i + 1], mesh.positions[i + 2]});
    return result;
}

std::vector<UvPositionAlg> normalsOf(const mc3togltf::MeshData& mesh) {
    std::vector<UvPositionAlg> result;
    result.reserve(mesh.normals.size() / 3);
    for (size_t i = 0; i + 2 < mesh.normals.size(); i += 3)
        result.push_back({mesh.normals[i], mesh.normals[i + 1], mesh.normals[i + 2]});
    return result;
}

std::vector<UvCoordinateAlg> coordinatesOf(const mc3togltf::MeshData& mesh) {
    std::vector<UvCoordinateAlg> result;
    result.reserve(mesh.texcoords.size() / 2);
    for (size_t i = 0; i + 1 < mesh.texcoords.size(); i += 2)
        result.push_back({mesh.texcoords[i], mesh.texcoords[i + 1]});
    return result;
}

bool equalCoordinates(const std::vector<UvCoordinateAlg>& lhs,
                      const std::vector<UvCoordinateAlg>& rhs)
{
    if (lhs.size() != rhs.size()) return false;
    for (size_t i = 0; i < lhs.size(); ++i)
        for (int component = 0; component < 2; ++component)
            if (std::fabs(lhs[i][component] - rhs[i][component]) > 1e-5f)
                return false;
    return true;
}

void compareWithExporter(const Mc3UvMapping& mapping, const std::string& label) {
    const Mc3Primitive primitive = Mc3Primitive::box({2.0f, 3.0f, 4.0f});
    const mc3togltf::MeshData original = mc3togltf::buildPrimitive(primitive);
    mc3togltf::MeshData exported = original;
    if (mapping.projection == UvProjection::Box) exported.applyBoxProjectionUv();
    else if (mapping.projection == UvProjection::Sphere) exported.applySphereProjectionUv();
    exported.applyUvMapping(mapping.scaleU, mapping.scaleV,
                            mapping.offsetU, mapping.offsetV, mapping.rotation);

    const auto viewport = mapObjectUvsAlg(positionsOf(original), normalsOf(original),
                                          coordinatesOf(original), mapping);
    check(equalCoordinates(viewport, coordinatesOf(exported)),
          label + " matches the independently generated glTF TEXCOORD_0 values");
}

} // namespace

int main() {
    Mc3UvMapping planar;
    planar.projection = UvProjection::Planar;
    planar.scaleU = 2.5f;
    planar.scaleV = -0.75f;
    planar.offsetU = 0.25f;
    planar.offsetV = -0.5f;
    planar.rotation = 30.0f;
    compareWithExporter(planar, "planar default unwrap + scale/rotate/offset");

    Mc3UvMapping box = planar;
    box.projection = UvProjection::Box;
    compareWithExporter(box, "box projection + scale/rotate/offset");

    Mc3UvMapping sphere = planar;
    sphere.projection = UvProjection::Sphere;
    compareWithExporter(sphere, "sphere projection + scale/rotate/offset");

    Mc3UvMapping transform;
    transform.scaleU = 2.0f;
    transform.scaleV = 3.0f;
    transform.offsetU = 0.5f;
    transform.offsetV = -1.0f;
    transform.rotation = 90.0f;
    const UvCoordinateAlg transformed = transformUvCoordinateAlg({1.0f, 0.0f}, transform);
    check(std::fabs(transformed[0] - 0.5f) < 1e-5f &&
          std::fabs(transformed[1] - 1.0f) < 1e-5f,
          "UV transform applies scale before rotation and offset after rotation");

    if (failures == 0) std::cout << "All UV mapping algorithm tests passed.\n";
    return failures == 0 ? 0 : 1;
}
