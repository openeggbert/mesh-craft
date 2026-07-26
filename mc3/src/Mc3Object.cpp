#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <cstdint>

namespace MeshCraft::Mc3 {

namespace {

std::shared_ptr<Mc3Object> makePrimitive(std::string name, Mc3Primitive prim,
                                          ObjectType otype, std::string material)
{
    auto obj = std::make_shared<Mc3Object>();
    obj->type      = otype;
    obj->name      = std::move(name);
    obj->primitive = std::move(prim);
    obj->material  = std::move(material);
    return obj;
}

// FNV-1a has a fully specified result, unlike std::hash<std::string>, whose
// value may differ between standard libraries. Variant resolution is part of
// the serialized Instance semantics, so it must be portable across editor and
// exporter builds.
std::uint64_t stableInstanceIdentityHash(std::string_view value)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char c : value) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

} // anonymous namespace

// --- Static factory implementations ---------------------------------------

std::shared_ptr<Mc3Object> Mc3Object::makeBox(
    std::string name, std::array<float,3> size, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::box(size),
                         ObjectType::Box, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeCube(
    std::string name, float side, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::cube(side),
                         ObjectType::Cube, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeSphere(
    std::string name, float radius, int segments, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::sphere(radius, segments),
                         ObjectType::Sphere, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeIcoSphere(
    std::string name, float radius, int subdivisions, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::icoSphere(radius, subdivisions),
                         ObjectType::IcoSphere, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeCylinder(
    std::string name, float radius, float height, int segments, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::cylinder(radius, height, segments),
                         ObjectType::Cylinder, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeCone(
    std::string name, float radius, float height, int segments, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::cone(radius, height, segments),
                         ObjectType::Cone, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makePlane(
    std::string name, float width, float depth, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::plane(width, depth),
                         ObjectType::Plane, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeTorus(
    std::string name, float majorRadius, float minorRadius, int segments, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::torus(majorRadius, minorRadius, segments),
                         ObjectType::Torus, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeCapsule(
    std::string name, float radius, float height, int segments, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::capsule(radius, height, segments),
                         ObjectType::Capsule, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeDisk(
    std::string name, float radius, int segments, std::string material)
{
    return makePrimitive(std::move(name), Mc3Primitive::disk(radius, segments),
                         ObjectType::Disk, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeGrid(
    std::string name, int subdivisionsX, int subdivisionsZ,
    float width, float depth, std::string material)
{
    return makePrimitive(std::move(name),
                         Mc3Primitive::grid(subdivisionsX, subdivisionsZ, width, depth),
                         ObjectType::Grid, std::move(material));
}

std::shared_ptr<Mc3Object> Mc3Object::makeGroup(
    std::string name, std::vector<std::shared_ptr<Mc3Object>> children)
{
    auto obj = std::make_shared<Mc3Object>();
    obj->type     = ObjectType::Group;
    obj->name     = std::move(name);
    obj->children = std::move(children);
    return obj;
}

std::shared_ptr<Mc3Object> Mc3Object::makeInstance(
    std::string name, std::string definition, std::string material)
{
    auto obj = std::make_shared<Mc3Object>();
    obj->type       = ObjectType::Instance;
    obj->name       = std::move(name);
    obj->definition = std::move(definition);
    obj->material   = std::move(material);
    return obj;
}

std::shared_ptr<Mc3Object> Mc3Object::makeInstanceVariant(
    std::string name, std::vector<std::string> definitions)
{
    auto obj = std::make_shared<Mc3Object>();
    obj->type               = ObjectType::Instance;
    obj->name               = std::move(name);
    if (!definitions.empty()) {
        obj->definition         = definitions.front();
        obj->variantDefinitions = std::move(definitions);
    }
    return obj;
}

std::shared_ptr<Mc3Object> Mc3Object::makeMesh(
    std::string name, std::string src, std::string material)
{
    auto obj = std::make_shared<Mc3Object>();
    obj->type       = ObjectType::Mesh;
    obj->name       = std::move(name);
    obj->meshSource = std::move(src);
    obj->material   = std::move(material);
    return obj;
}

std::shared_ptr<Mc3Object> Mc3Object::makeUnion(
    std::string name, std::vector<std::shared_ptr<Mc3Object>> children)
{
    auto obj = std::make_shared<Mc3Object>();
    obj->type         = ObjectType::Union;
    obj->name         = std::move(name);
    obj->csgOperation = Mc3CsgOperation{CsgType::Union};
    obj->children     = std::move(children);
    return obj;
}

std::shared_ptr<Mc3Object> Mc3Object::makeDifference(
    std::string name, std::vector<std::shared_ptr<Mc3Object>> children)
{
    auto obj = std::make_shared<Mc3Object>();
    obj->type         = ObjectType::Difference;
    obj->name         = std::move(name);
    obj->csgOperation = Mc3CsgOperation{CsgType::Difference};
    obj->children     = std::move(children);
    return obj;
}

std::shared_ptr<Mc3Object> Mc3Object::makeIntersection(
    std::string name, std::vector<std::shared_ptr<Mc3Object>> children)
{
    auto obj = std::make_shared<Mc3Object>();
    obj->type         = ObjectType::Intersection;
    obj->name         = std::move(name);
    obj->csgOperation = Mc3CsgOperation{CsgType::Intersection};
    obj->children     = std::move(children);
    return obj;
}

// --- Fluent setter implementations ----------------------------------------

Mc3Object& Mc3Object::at(float x, float y, float z) {
    transform.position = {x, y, z}; return *this;
}
Mc3Object& Mc3Object::rotatedBy(float rx, float ry, float rz) {
    transform.rotation = {rx, ry, rz}; return *this;
}
Mc3Object& Mc3Object::scaledBy(float sx, float sy, float sz) {
    transform.scale = {sx, sy, sz}; return *this;
}
Mc3Object& Mc3Object::scaledBy(float s) {
    transform.scale = {s, s, s}; return *this;
}
Mc3Object& Mc3Object::withId(std::string newId) {
    id = std::move(newId); return *this;
}
Mc3Object& Mc3Object::withMaterial(std::string mat) {
    material = std::move(mat); return *this;
}
Mc3Object& Mc3Object::withVisible(bool v) {
    visible = v; return *this;
}
Mc3Object& Mc3Object::withCollision(std::string c) {
    collision = std::move(c); return *this;
}
Mc3Object& Mc3Object::withLayer(std::string l) {
    layer = std::move(l); return *this;
}
Mc3Object& Mc3Object::withTag(std::string tag) {
    tags.push_back(std::move(tag)); return *this;
}
Mc3Object& Mc3Object::withDeform(float sx, float sy, float sz) {
    deform = Mc3Deform{{sx, sy, sz}}; return *this;
}
Mc3Object& Mc3Object::addChild(std::shared_ptr<Mc3Object> child) {
    children.push_back(std::move(child)); return *this;
}
Mc3Object& Mc3Object::asCutter(bool v) {
    isCutter = v; return *this;
}
Mc3Object& Mc3Object::withUvMapping(Mc3UvMapping uv) {
    uvMapping = std::move(uv); return *this;
}
Mc3Object& Mc3Object::withUvMapping(UvProjection proj, float scaleU, float scaleV) {
    uvMapping = Mc3UvMapping{proj, scaleU, scaleV}; return *this;
}
Mc3Object& Mc3Object::withMetadata(std::string key, std::string value) {
    metadata[std::move(key)] = std::move(value); return *this;
}

const std::string& Mc3Object::resolvedInstanceDefinitionKey() const {
    if (!variantDefinitions.empty()) {
        std::size_t idx = stableInstanceIdentityHash(id) % variantDefinitions.size();
        return variantDefinitions[idx];
    }
    return definition;
}

} // namespace MeshCraft::Mc3
