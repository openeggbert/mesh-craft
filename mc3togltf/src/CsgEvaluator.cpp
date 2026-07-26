#include "CsgEvaluator.hpp"
#include "MeshBuilder.hpp"

#include <MeshCraft/AssetLodAlgorithms.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include <manifold/manifold.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <unordered_map>

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
// Convert Manifold → MeshData. Manifold calculates vertex normals after the
// boolean, retaining a 60-degree crease threshold: curved inputs shade
// smoothly while cube-like edges remain hard. Unlike the former triangle-soup
// path, the returned indexed geometry shares these normal-aware vertices.
// ---------------------------------------------------------------------------

static MeshData meshDataFromCsgMeshGl(const manifold::MeshGL& gl) {
    MeshData result;
    int np    = static_cast<int>(gl.numProp);
    int nVerts = np > 0 ? static_cast<int>(gl.vertProperties.size()) / np : 0;
    int nTris = static_cast<int>(gl.triVerts.size()) / 3;
    if (nTris <= 0 || nVerts <= 0 || np < 6) return result;

    result.positions.reserve(static_cast<size_t>(nVerts) * 3);
    result.normals.reserve(static_cast<size_t>(nVerts) * 3);
    result.indices.reserve(gl.triVerts.size());
    for (int vertex = 0; vertex < nVerts; ++vertex) {
        const size_t offset = static_cast<size_t>(vertex) * np;
        result.positions.insert(result.positions.end(), {
            gl.vertProperties[offset + 0], gl.vertProperties[offset + 1], gl.vertProperties[offset + 2]});
        result.normals.insert(result.normals.end(), {
            gl.vertProperties[offset + 3], gl.vertProperties[offset + 4], gl.vertProperties[offset + 5]});
    }
    result.indices.assign(gl.triVerts.begin(), gl.triVerts.end());

    return result;
}

MeshData meshDataFromCsgManifold(const manifold::Manifold& mfd) {
    if (mfd.IsEmpty()) return {};
    return meshDataFromCsgMeshGl(
        mfd.CalculateNormals(/*normalIdx=*/0).GetMeshGL(/*normalIdx=*/0));
}

static CsgMeshData manifoldToCsgMeshData(
    const manifold::Manifold& mfd,
    const std::unordered_map<uint32_t, std::string>& materialByOriginal)
{
    CsgMeshData result;
    // CalculateNormals() changes the vertex-property layout but preserves the
    // Manifold relation runs. The runs are sorted by source original ID and
    // cover every output triangle, including newly-created boolean cut faces.
    const manifold::MeshGL gl = mfd.CalculateNormals(/*normalIdx=*/0).GetMeshGL(/*normalIdx=*/0);
    result.mesh = meshDataFromCsgMeshGl(gl);
    if (result.mesh.empty()) return result;
    const size_t triCount = result.mesh.indices.size() / 3;
    result.triangleMaterials.assign(triCount, {});
    if (gl.runIndex.size() != gl.runOriginalID.size() + 1) return result;
    for (size_t run = 0; run < gl.runOriginalID.size(); ++run) {
        const size_t begin = gl.runIndex[run] / 3;
        const size_t end = gl.runIndex[run + 1] / 3;
        if (begin > end || end > triCount) continue;
        auto material = materialByOriginal.find(gl.runOriginalID[run]);
        if (material == materialByOriginal.end()) continue;
        for (size_t tri = begin; tri < end; ++tri)
            result.triangleMaterials[tri] = material->second;
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
    const std::string& csgRootName,
    const std::string& rootMaterial,
    std::unordered_map<uint32_t, std::string>& materialByOriginal)
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

    auto applyXf = [&](Manifold m) {
        // Manifold's analytic constructors and MeshGL constructor both assign
        // an OriginalID. Record it before Transform() turns this leaf into a
        // product manifold; GetMeshGL() later uses the surviving relation to
        // partition the boolean result into material primitives.
        const int originalId = m.OriginalID();
        if (originalId >= 0) {
            const std::string& material = !obj.materialOverride.empty() ? obj.materialOverride
                                        : !obj.material.empty() ? obj.material
                                        : rootMaterial;
            materialByOriginal.emplace(static_cast<uint32_t>(originalId), material);
        }
        return m.Transform(xf);
    };

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
        // STAB-0670 follow-up: buildPrimitive() duplicates vertices at UV
        // seams (correct for rendering, but the raw triangle soup is
        // non-manifold) -- without welding colocated verts first, EVERY
        // Torus/Capsule/IcoSphere CSG child hit "non-manifold geometry"
        // here, a real, previously-undiscovered export failure (no fixture
        // had ever exercised these types as CSG children until now).
        //
        // STAB-0700: Torus still failed NotManifold even after Merge() --
        // root-caused via a standalone probe: MeshGL::Merge()'s DEFAULT
        // tolerance (0, meaning "use its own auto-computed bounding-box-
        // derived baseline") is too tight for a torus's seam vertices, so
        // it only found 40 of the 49 actually-needed weld pairs (confirmed
        // directly: none of the seam positions are bit-identical due to
        // ordinary floating-point trig roundoff, so this is purely a
        // tolerance-too-small issue, not a genuine topological defect).
        // Explicitly setting a small absolute tolerance before Merge()
        // finds all 49 pairs and produces a valid manifold. Verified this
        // value is safe (doesn't over- or under-merge) across scales
        // spanning 100x (a torus with major_radius=0.004 still merges
        // exactly 49 pairs, not more/fewer) and doesn't regress the
        // already-working Capsule/IcoSphere cases.
        gl.tolerance = 1e-4f;
        gl.Merge();
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
            if (child) result = result + buildManifoldNode(*child, definitions, nodeMat, depth + 1,
                                                            csgRootName, rootMaterial, materialByOriginal);
        return result;
    }
    case ObjectType::Intersection: {
        // Every other nested-CSG case (Union/Difference/Group/Area) guards
        // EVERY child with `if (child)`; this one used to seed `result` from
        // obj.children[0] unconditionally, dereferencing it even if it were
        // null. No current parser path produces a null child, but this finds
        // the first non-null child to seed from instead of assuming index 0
        // is always populated, matching the guard used everywhere else.
        size_t first = 0;
        while (first < obj.children.size() && !obj.children[first]) ++first;
        if (first >= obj.children.size()) return Manifold{};
        Manifold result = buildManifoldNode(*obj.children[first], definitions, nodeMat, depth + 1,
                                             csgRootName, rootMaterial, materialByOriginal);
        for (size_t i = first + 1; i < obj.children.size(); ++i)
            if (obj.children[i])
                result = result ^ buildManifoldNode(*obj.children[i], definitions, nodeMat, depth + 1,
                                                     csgRootName, rootMaterial, materialByOriginal);
        return result;
    }
    case ObjectType::Difference: {
        bool hasBase = false;
        Manifold base;
        for (const auto& child : obj.children)
            if (child && !child->isCutter) {
                base = base + buildManifoldNode(*child, definitions, nodeMat, depth + 1,
                                                 csgRootName, rootMaterial, materialByOriginal);
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
                base = base - buildManifoldNode(*child, definitions, nodeMat, depth + 1,
                                                 csgRootName, rootMaterial, materialByOriginal);
        return base;
    }

    // --- Group / Area: union of children ---
    case ObjectType::Group:
    case ObjectType::Area: {
        Manifold result;
        for (const auto& child : obj.children)
            if (child) result = result + buildManifoldNode(*child, definitions, nodeMat, depth + 1,
                                                            csgRootName, rootMaterial, materialByOriginal);
        return result;
    }

    // --- Instance: resolve definition ---
    case ObjectType::Instance: {
        // CSG export has no camera distance either. Match GltfExporter.cpp's
        // ordinary Instance path by baking the explicit Near/default authored
        // tier, rather than making CSG a hidden exception to asset LOD.
        const auto assetLod = MeshCraft::resolveDefaultAssetLodForInstanceAlg(obj, definitions);
        const std::string& defKey = assetLod.definitionId;
        auto it = definitions.find(defKey);
        if (it != definitions.end() && it->second)
            return buildManifoldNode(*it->second, definitions, nodeMat, depth + 1,
                                     csgRootName, rootMaterial, materialByOriginal);
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

CsgMeshData evaluateCsgNodeWithMaterials(
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
    const std::string& rootMaterial = !csgObj.materialOverride.empty() ? csgObj.materialOverride
                                    : csgObj.material;
    std::unordered_map<uint32_t, std::string> materialByOriginal;

    Manifold result;

    if (csgObj.type == ObjectType::Union) {
        for (const auto& child : csgObj.children)
            if (child) result = result + buildManifoldNode(*child, definitions, identity, 0,
                                                            rootName, rootMaterial, materialByOriginal);

    } else if (csgObj.type == ObjectType::Difference) {
        bool hasBase = false;
        Manifold base;
        for (const auto& child : csgObj.children)
            if (child && !child->isCutter) {
                base = base + buildManifoldNode(*child, definitions, identity, 0,
                                                 rootName, rootMaterial, materialByOriginal);
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
                base = base - buildManifoldNode(*child, definitions, identity, 0,
                                                 rootName, rootMaterial, materialByOriginal);
        result = base;

    } else { // Intersection
        bool first = true;
        for (const auto& child : csgObj.children) {
            if (!child) continue;
            Manifold m = buildManifoldNode(*child, definitions, identity, 0,
                                            rootName, rootMaterial, materialByOriginal);
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

    return manifoldToCsgMeshData(result, materialByOriginal);
}

MeshData evaluateCsgNode(
    const Mc3Object& csgObj,
    const std::map<std::string, std::shared_ptr<Mc3Object>>& definitions)
{
    return evaluateCsgNodeWithMaterials(csgObj, definitions).mesh;
}

} // namespace mc3togltf
