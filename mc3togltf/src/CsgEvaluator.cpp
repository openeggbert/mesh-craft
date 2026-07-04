#include "CsgEvaluator.hpp"
#include "MeshBuilder.hpp"

#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include <manifold/manifold.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

using namespace MeshCraft::Mc3;

namespace mc3togltf {

// ---------------------------------------------------------------------------
// Column-major 4×4 matrix — no XNA / CNA dependency.
// Convention: v' = M * v  (column vectors, right-to-left application order).
// ---------------------------------------------------------------------------

struct Mat4 {
    // m[col][row], zero-initialised.
    float m[4][4]{};

    static Mat4 identity() {
        Mat4 r{};
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    static Mat4 translation(float tx, float ty, float tz) {
        Mat4 r = identity();
        r.m[3][0] = tx; r.m[3][1] = ty; r.m[3][2] = tz;
        return r;
    }

    static Mat4 scaling(float sx, float sy, float sz) {
        Mat4 r = identity();
        r.m[0][0] = sx; r.m[1][1] = sy; r.m[2][2] = sz;
        return r;
    }

    // Extrinsic XYZ Euler (degrees): R = Rz * Ry * Rx.
    // Matches the eulerXYZToQuat convention used for non-CSG glTF node rotations.
    static Mat4 rotationXYZ(float xDeg, float yDeg, float zDeg) {
        constexpr float d = std::numbers::pi_v<float> / 180.0f;
        float cx = std::cos(xDeg * d), sx = std::sin(xDeg * d);
        float cy = std::cos(yDeg * d), sy = std::sin(yDeg * d);
        float cz = std::cos(zDeg * d), sz = std::sin(zDeg * d);

        // Columns of Rz * Ry * Rx (where each col is where a basis vector maps to):
        Mat4 r{};
        // col 0: where X=(1,0,0) maps to
        r.m[0][0] =  cz * cy;
        r.m[0][1] =  sz * cy;
        r.m[0][2] = -sy;
        // col 1: where Y=(0,1,0) maps to
        r.m[1][0] =  cz * sy * sx - sz * cx;
        r.m[1][1] =  sz * sy * sx + cz * cx;
        r.m[1][2] =  cy * sx;
        // col 2: where Z=(0,0,1) maps to
        r.m[2][0] =  cz * sy * cx + sz * sx;
        r.m[2][1] =  sz * sy * cx - cz * sx;
        r.m[2][2] =  cy * cx;
        r.m[3][3] = 1.0f;
        return r;
    }

    // M = *this * rhs  (rhs applied first, then *this).
    Mat4 operator*(const Mat4& rhs) const {
        Mat4 result{};
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row)
                for (int k = 0; k < 4; ++k)
                    result.m[c][row] += m[k][row] * rhs.m[c][k];
        return result;
    }

    // Convert to Manifold's affine 3×4 matrix (column-major, double-precision).
    manifold::mat3x4 toManifold() const {
        return manifold::mat3x4(
            manifold::vec3{m[0][0], m[0][1], m[0][2]},
            manifold::vec3{m[1][0], m[1][1], m[1][2]},
            manifold::vec3{m[2][0], m[2][1], m[2][2]},
            manifold::vec3{m[3][0], m[3][1], m[3][2]}
        );
    }
};

// ---------------------------------------------------------------------------
// Build the local-to-parent transform for an MC3 object (SRT + pivot + deform).
// ---------------------------------------------------------------------------

static Mat4 computeObjMat(const Mc3Object& obj) {
    const auto& t = obj.transform;
    float px = t.pivot[0], py = t.pivot[1], pz = t.pivot[2];

    Mat4 srt = Mat4::translation(t.position[0] + px,
                                 t.position[1] + py,
                                 t.position[2] + pz)
             * Mat4::rotationXYZ(t.rotation[0], t.rotation[1], t.rotation[2])
             * Mat4::scaling(t.scale[0], t.scale[1], t.scale[2])
             * Mat4::translation(-px, -py, -pz);

    if (obj.deform) {
        Mat4 def = Mat4::scaling(obj.deform->scale[0],
                                 obj.deform->scale[1],
                                 obj.deform->scale[2]);
        return srt * def;   // deform applied first (innermost)
    }
    return srt;
}

// ---------------------------------------------------------------------------
// Convert Manifold → MeshData (flat face normals, no UVs).
// ---------------------------------------------------------------------------

static MeshData manifoldToMeshData(const manifold::Manifold& mfd) {
    MeshData result;
    if (mfd.IsEmpty()) return result;

    manifold::MeshGL gl = mfd.GetMeshGL();
    int np    = static_cast<int>(gl.numProp);
    int nTris = static_cast<int>(gl.triVerts.size()) / 3;
    if (nTris <= 0 || np < 3) return result;

    result.positions.reserve(static_cast<size_t>(nTris) * 9);
    result.normals  .reserve(static_cast<size_t>(nTris) * 9);
    result.texcoords.reserve(static_cast<size_t>(nTris) * 6);
    result.indices  .reserve(static_cast<size_t>(nTris) * 3);

    for (int t = 0; t < nTris; ++t) {
        auto i0 = static_cast<size_t>(gl.triVerts[3 * t + 0]);
        auto i1 = static_cast<size_t>(gl.triVerts[3 * t + 1]);
        auto i2 = static_cast<size_t>(gl.triVerts[3 * t + 2]);

        float x0 = gl.vertProperties[i0 * np + 0], y0 = gl.vertProperties[i0 * np + 1], z0 = gl.vertProperties[i0 * np + 2];
        float x1 = gl.vertProperties[i1 * np + 0], y1 = gl.vertProperties[i1 * np + 1], z1 = gl.vertProperties[i1 * np + 2];
        float x2 = gl.vertProperties[i2 * np + 0], y2 = gl.vertProperties[i2 * np + 1], z2 = gl.vertProperties[i2 * np + 2];

        // Face normal via cross product
        float ax = x1 - x0, ay = y1 - y0, az = z1 - z0;
        float bx = x2 - x0, by = y2 - y0, bz = z2 - z0;
        float nx = ay * bz - az * by;
        float ny = az * bx - ax * bz;
        float nz = ax * by - ay * bx;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-10f) { nx /= len; ny /= len; nz /= len; }

        auto base = static_cast<uint32_t>(result.positions.size() / 3);
        result.positions.insert(result.positions.end(), {x0,y0,z0, x1,y1,z1, x2,y2,z2});
        result.normals  .insert(result.normals  .end(), {nx,ny,nz, nx,ny,nz, nx,ny,nz});
        result.texcoords.insert(result.texcoords.end(), {0,0, 0,0, 0,0});
        result.indices  .insert(result.indices  .end(), {base, base+1, base+2});
    }

    return result;
}

// ---------------------------------------------------------------------------
// Recursive Manifold tree builder.
// parentMat: accumulated transform from the CSG root's local space.
//            Starts as identity for direct children of the CSG root.
// ---------------------------------------------------------------------------

static constexpr int CSG_MAX_DEPTH = 12;
static constexpr int CSG_SEGMENTS  = 32;

static manifold::Manifold buildManifoldNode(
    const Mc3Object& obj,
    const std::map<std::string, std::shared_ptr<Mc3Object>>& definitions,
    const Mat4& parentMat,
    int depth,
    const std::string& csgRootName)
{
    using namespace manifold;

    if (depth > CSG_MAX_DEPTH)
        throw std::runtime_error(
            std::string("CSG evaluation failed for '") + csgRootName +
            "': nesting depth exceeds " + std::to_string(CSG_MAX_DEPTH) +
            " levels (possible infinite recursion or excessively deep hierarchy).\n"
            "Use --allow-approximate-csg to export children separately as a debug fallback.");

    if (!obj.visible) return Manifold{};

    // Accumulated transform: parentMat * this object's SRT
    Mat4 nodeMat = parentMat * computeObjMat(obj);
    auto xf = nodeMat.toManifold();

    auto applyXf = [&](Manifold m) { return m.Transform(xf); };

    switch (obj.type) {
    // --- Analytic primitives ---
    case ObjectType::Box:
    case ObjectType::Cube: {
        float w = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float h = obj.primitive ? obj.primitive->size[1] : 1.0f;
        float d = obj.primitive ? obj.primitive->size[2] : 1.0f;
        return applyXf(Manifold::Cube({w, h, d}, /*center=*/true));
    }
    case ObjectType::Sphere: {
        float r = obj.primitive ? obj.primitive->radius : 0.5f;
        return applyXf(Manifold::Sphere(r, CSG_SEGMENTS));
    }
    case ObjectType::Cylinder: {
        float r = obj.primitive ? obj.primitive->radius : 0.5f;
        float h = obj.primitive ? obj.primitive->height : 1.0f;
        return applyXf(Manifold::Cylinder(h, r, r, CSG_SEGMENTS, /*center=*/true));
    }
    case ObjectType::Cone: {
        float r = obj.primitive ? obj.primitive->radius : 0.5f;
        float h = obj.primitive ? obj.primitive->height : 1.0f;
        return applyXf(Manifold::Cylinder(h, r, 0.0f, CSG_SEGMENTS, /*center=*/true));
    }

    // --- Triangulated watertight primitives ---
    case ObjectType::Torus:
    case ObjectType::Capsule:
    case ObjectType::IcoSphere: {
        const char* typeName = (obj.type == ObjectType::Torus)     ? "Torus"
                             : (obj.type == ObjectType::Capsule)   ? "Capsule"
                             :                                        "IcoSphere";
        auto failMsg = [&](const char* reason) {
            return std::string("CSG evaluation failed for '") + csgRootName +
                   "':\nchild '" + obj.name + "' of type " + typeName +
                   " — " + reason + ".\n"
                   "Use --allow-approximate-csg to export children separately as a debug fallback.";
        };
        if (!obj.primitive)
            throw std::runtime_error(failMsg("missing primitive data"));
        MeshData md = buildPrimitive(*obj.primitive);
        if (md.empty() || md.positions.empty() || md.indices.empty())
            throw std::runtime_error(failMsg("buildPrimitive produced empty geometry"));
        MeshGL gl;
        gl.numProp = 3;
        gl.vertProperties.assign(md.positions.begin(), md.positions.end());
        gl.triVerts.assign(md.indices.begin(), md.indices.end());
        Manifold m(gl);
        if (m.Status() != Manifold::Error::NoError)
            throw std::runtime_error(failMsg("triangulation produced non-manifold geometry"));
        return applyXf(m);
    }

    // --- Non-watertight primitives: hard failure in real CSG mode ---
    case ObjectType::Plane:
    case ObjectType::Disk:
    case ObjectType::Grid: {
        const char* typeName = (obj.type == ObjectType::Plane) ? "Plane"
                             : (obj.type == ObjectType::Disk)  ? "Disk"
                             :                                   "Grid";
        throw std::runtime_error(
            std::string("CSG evaluation failed for '") + csgRootName +
            "':\nchild '" + obj.name + "' of type " + typeName +
            " is not watertight and cannot be used in real CSG export.\n"
            "Use --allow-approximate-csg to export children separately as a debug fallback.");
    }

    // --- Complex geometry: hard failure in real CSG mode ---
    case ObjectType::Mesh:
    case ObjectType::Extrude: {
        const char* typeName = (obj.type == ObjectType::Mesh) ? "Mesh" : "Extrude";
        throw std::runtime_error(
            std::string("CSG evaluation failed for '") + csgRootName +
            "':\nchild '" + obj.name + "' of type " + typeName +
            " is not supported in real CSG export.\n"
            "Use --allow-approximate-csg to export children separately as a debug fallback.");
    }

    // --- CSG nodes (nested) ---
    case ObjectType::Union: {
        Manifold result;
        for (const auto& child : obj.children)
            if (child) result = result + buildManifoldNode(*child, definitions, nodeMat, depth + 1, csgRootName);
        return result;
    }
    case ObjectType::Intersection: {
        if (obj.children.empty()) return Manifold{};
        Manifold result = buildManifoldNode(*obj.children[0], definitions, nodeMat, depth + 1, csgRootName);
        for (size_t i = 1; i < obj.children.size(); ++i)
            if (obj.children[i])
                result = result ^ buildManifoldNode(*obj.children[i], definitions, nodeMat, depth + 1, csgRootName);
        return result;
    }
    case ObjectType::Difference: {
        bool hasBase = false;
        Manifold base;
        for (const auto& child : obj.children)
            if (child && !child->isCutter) {
                base = base + buildManifoldNode(*child, definitions, nodeMat, depth + 1, csgRootName);
                hasBase = true;
            }
        if (!hasBase)
            throw std::runtime_error(
                std::string("CSG evaluation failed for '") + csgRootName +
                "':\nnested difference node '" + obj.name +
                "' has no non-cutter children — base volume is empty.\n"
                "Use --allow-approximate-csg to export children separately as a debug fallback.");
        for (const auto& child : obj.children)
            if (child && child->isCutter)
                base = base - buildManifoldNode(*child, definitions, nodeMat, depth + 1, csgRootName);
        return base;
    }

    // --- Group / Area: union of children ---
    case ObjectType::Group:
    case ObjectType::Area: {
        Manifold result;
        for (const auto& child : obj.children)
            if (child) result = result + buildManifoldNode(*child, definitions, nodeMat, depth + 1, csgRootName);
        return result;
    }

    // --- Instance: resolve definition ---
    case ObjectType::Instance: {
        const std::string& defKey = obj.resolvedInstanceDefinitionKey();
        auto it = definitions.find(defKey);
        if (it != definitions.end() && it->second)
            return buildManifoldNode(*it->second, definitions, nodeMat, depth + 1, csgRootName);
        throw std::runtime_error(
            std::string("CSG evaluation failed for '") + csgRootName +
            "':\ninstance '" + obj.name + "' references unknown definition '" + defKey +
            "'.\nUse --allow-approximate-csg to export children separately as a debug fallback.");
    }

    default:
        throw std::runtime_error(
            std::string("CSG evaluation failed for '") + csgRootName +
            "':\nchild '" + obj.name +
            "' has an object type that is not supported in real CSG export.\n"
            "Use --allow-approximate-csg to export children separately as a debug fallback.");
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

MeshData evaluateCsgNode(
    const Mc3Object& csgObj,
    const std::map<std::string, std::shared_ptr<Mc3Object>>& definitions)
{
    using namespace manifold;

    if (csgObj.type != ObjectType::Union      &&
        csgObj.type != ObjectType::Difference &&
        csgObj.type != ObjectType::Intersection)
        throw std::runtime_error("evaluateCsgNode called on non-CSG node '" + csgObj.name + "'");

    // Start with identity: children are evaluated in the CSG root's local space.
    // The CSG root's own transform is carried by the glTF node TRS, not baked here.
    const Mat4 identity = Mat4::identity();
    const std::string& rootName = csgObj.name;

    Manifold result;

    if (csgObj.type == ObjectType::Union) {
        for (const auto& child : csgObj.children)
            if (child) result = result + buildManifoldNode(*child, definitions, identity, 0, rootName);

    } else if (csgObj.type == ObjectType::Difference) {
        bool hasBase = false;
        Manifold base;
        for (const auto& child : csgObj.children)
            if (child && !child->isCutter) {
                base = base + buildManifoldNode(*child, definitions, identity, 0, rootName);
                hasBase = true;
            }
        if (!hasBase)
            throw std::runtime_error(
                std::string("CSG evaluation failed for '") + rootName +
                "': difference node has no non-cutter children — base volume is empty.\n"
                "Add at least one child without role=\"cutter\", or use "
                "--allow-approximate-csg as a debug fallback.");
        for (const auto& child : csgObj.children)
            if (child && child->isCutter)
                base = base - buildManifoldNode(*child, definitions, identity, 0, rootName);
        result = base;

    } else { // Intersection
        bool first = true;
        for (const auto& child : csgObj.children) {
            if (!child) continue;
            Manifold m = buildManifoldNode(*child, definitions, identity, 0, rootName);
            if (first) { result = m; first = false; }
            else        result = result ^ m;
        }
    }

    if (result.Status() != Manifold::Error::NoError) {
        throw std::runtime_error(
            std::string("CSG evaluation failed for '") + rootName +
            "': Manifold returned error code " +
            std::to_string(static_cast<int>(result.Status())) + ".\n"
            "Use --allow-approximate-csg to export children separately as a debug fallback.");
    }

    if (result.IsEmpty()) {
        std::cerr << "Warning: mc3togltf CSG: '" << rootName
                  << "' — boolean operation produced an empty volume "
                     "(no geometry; check for non-overlapping inputs).\n";
    }

    return manifoldToMeshData(result);
}

} // namespace mc3togltf
