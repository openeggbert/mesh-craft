#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include "GltfExporter.hpp"
#include "CsgEvaluator.hpp"
#include "MathUtils.hpp"
#include "MeshBuilder.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Light.hpp>
#include <MeshCraft/Mc3/Mc3Camera.hpp>
#include <MeshCraft/Mc3/Mc3Environment.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numbers>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <fstream>
#include <unordered_map>
#include <vector>

using namespace MeshCraft::Mc3;
namespace mc3togltf {

OutputFormat outputFormatFromPath(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".glb")  return OutputFormat::GLB;
    if (ext == ".gltf") return OutputFormat::GLTF;
    throw std::runtime_error(
        std::string("Unknown output extension '") + path.extension().string() +
        "'. Only .gltf and .glb are supported.");
}

// ---------------------------------------------------------------------------
// Export context (passed through recursive node building)
// ---------------------------------------------------------------------------

struct ExportCtx {
    tinygltf::Model& model;
    const std::unordered_map<std::string, int>& matNameToIdx;
    const std::map<std::string, std::shared_ptr<Mc3Object>>& definitions;
    float unitScale{1.0f};           // conversion factor to metres
    std::filesystem::path basePath;  // directory of source .mc3.xml (for OBJ paths)
    bool allowApproximateCSG{false};

    // Cache key → glTF mesh index for MC3 <instance> nodes that share a definition.
    // Key built by buildDefCacheKey(): includes definition ID, material, and deform scale.
    // Instances with identical geometry (same def + mat + deform) share one glTF mesh.
    std::map<std::string, int> defMeshCache;

    // Cache geometry_key → glTF mesh index — avoids duplicate geometry for repeated
    // primitives (boxes, spheres, cylinders…), OBJ meshes, and extrude shapes.
    std::map<std::string, int> geomMeshCache;

    // Accumulated export statistics — copied to GltfExporter::stats after export.
    ExportStats stats;
};

// ---------------------------------------------------------------------------
// Instance cache key: definition ID + effective material + optional deform scale.
// Two instances with different deform must NOT share a glTF mesh because deform
// changes vertex positions before they are baked into the buffer.
// ---------------------------------------------------------------------------

static std::string buildDefCacheKey(const std::string& defId, int matIdx,
                                     const std::optional<Mc3Deform>& deform) {
    std::ostringstream k;
    k << defId << '|' << matIdx;
    if (deform.has_value()) {
        k << "|D" << std::setprecision(std::numeric_limits<float>::max_digits10)
          << deform->scale[0] << ',' << deform->scale[1] << ',' << deform->scale[2];
    }
    return k.str();
}

// ---------------------------------------------------------------------------
// Geometry cache key: serialise all parameters that affect vertex/index data.
// Uses max_digits10 float precision so close-but-distinct float values produce
// distinct keys and do not incorrectly share the same glTF mesh.
// Includes deform (which modifies vertex positions) and material (baked into
// the glTF primitive).  Does NOT include node-level transform (handled as TRS).
// ---------------------------------------------------------------------------

static std::string buildGeomCacheKey(const Mc3Object& obj, int matIdx) {
    std::ostringstream k;
    k << std::setprecision(std::numeric_limits<float>::max_digits10);
    k << static_cast<int>(obj.type) << '|';

    if (obj.type == ObjectType::Mesh) {
        k << obj.meshSource;
    } else if (obj.primitive.has_value()) {
        const auto& p = *obj.primitive;
        k << p.size[0]        << ',' << p.size[1]       << ',' << p.size[2]      << '|'
          << p.radius          << '|' << p.height         << '|' << p.segments     << '|'
          << p.axis            << '|' << p.majorRadius    << '|' << p.minorRadius  << '|'
          << p.subdivisionsX   << '|' << p.subdivisionsZ;
    } else if (obj.extrude.has_value()) {
        const auto& e  = *obj.extrude;
        const auto& cs = e.crossSection;
        const auto& pt = e.path;
        k << static_cast<int>(cs.type) << ','
          << cs.width   << ',' << cs.height      << ','
          << cs.radius  << ',' << cs.innerRadius  << ','
          << cs.sides   << ',' << cs.segments     << ',';
        for (const auto& cp : cs.customPoints)
            k << cp.x << '.' << cp.y << ';';
        k << '|' << static_cast<int>(pt.type) << ','
          << pt.length      << ',' << pt.axis       << ','
          << pt.arcRadius   << ',' << pt.arcAngle   << ','
          << pt.helixRadius << ',' << pt.helixHeight << ',' << pt.helixTurns << ',';
        for (const auto& pp : pt.points)
            k << pp.position[0]  << ',' << pp.position[1]  << ',' << pp.position[2]  << ','
              << pp.controlIn[0] << ',' << pp.controlIn[1] << ',' << pp.controlIn[2] << ';';
        k << '|' << e.twist << ',' << e.segments << ',' << e.smooth << ',' << e.caps;
    }

    if (obj.deform.has_value()) {
        const auto& d = *obj.deform;
        k << "|D" << d.scale[0] << ',' << d.scale[1] << ',' << d.scale[2];
    }

    k << "|M" << matIdx;
    return k.str();
}

static float unitScaleFactor(const std::string& unit) {
    if (unit == "centimeter")  return 0.01f;
    if (unit == "millimeter")  return 0.001f;
    if (unit == "inch")        return 0.0254f;
    if (unit == "foot")        return 0.3048f;
    if (unit == "kilometer")   return 1000.0f;
    return 1.0f; // "meter" or unknown
}

// Forward declaration
static int buildNode(ExportCtx& ctx, const Mc3Object& obj);

// ---------------------------------------------------------------------------
// Buffer helpers
// ---------------------------------------------------------------------------

static int addBufferView(tinygltf::Model& model,
                         const void* data, size_t byteLen,
                         int target)
{
    auto& buf = model.buffers[0];
    size_t offset = buf.data.size();
    buf.data.resize(offset + byteLen);
    std::memcpy(buf.data.data() + offset, data, byteLen);

    tinygltf::BufferView bv;
    bv.buffer     = 0;
    bv.byteOffset = static_cast<int>(offset);
    bv.byteLength = static_cast<int>(byteLen);
    bv.target     = target;
    model.bufferViews.push_back(std::move(bv));
    return static_cast<int>(model.bufferViews.size()) - 1;
}

static int addAccessorVec3(tinygltf::Model& model,
                           const std::vector<float>& data,
                           bool calcBounds = false)
{
    int bvIdx = addBufferView(model, data.data(),
                              data.size() * sizeof(float),
                              TINYGLTF_TARGET_ARRAY_BUFFER);

    tinygltf::Accessor acc;
    acc.bufferView    = bvIdx;
    acc.byteOffset    = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.count         = static_cast<int>(data.size() / 3);
    acc.type          = TINYGLTF_TYPE_VEC3;

    if (calcBounds && !data.empty()) {
        float minX = data[0], minY = data[1], minZ = data[2];
        float maxX = minX,    maxY = minY,    maxZ = minZ;
        for (size_t i = 0; i < data.size(); i += 3) {
            if (data[i]   < minX) minX = data[i];
            if (data[i+1] < minY) minY = data[i+1];
            if (data[i+2] < minZ) minZ = data[i+2];
            if (data[i]   > maxX) maxX = data[i];
            if (data[i+1] > maxY) maxY = data[i+1];
            if (data[i+2] > maxZ) maxZ = data[i+2];
        }
        acc.minValues = {minX, minY, minZ};
        acc.maxValues = {maxX, maxY, maxZ};
    }

    model.accessors.push_back(std::move(acc));
    return static_cast<int>(model.accessors.size()) - 1;
}

static int addAccessorVec2(tinygltf::Model& model,
                           const std::vector<float>& data)
{
    int bvIdx = addBufferView(model, data.data(),
                              data.size() * sizeof(float),
                              TINYGLTF_TARGET_ARRAY_BUFFER);

    tinygltf::Accessor acc;
    acc.bufferView    = bvIdx;
    acc.byteOffset    = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.count         = static_cast<int>(data.size() / 2);
    acc.type          = TINYGLTF_TYPE_VEC2;
    model.accessors.push_back(std::move(acc));
    return static_cast<int>(model.accessors.size()) - 1;
}

static int addAccessorIndices(tinygltf::Model& model,
                              const std::vector<uint32_t>& indices)
{
    int bvIdx = addBufferView(model, indices.data(),
                              indices.size() * sizeof(uint32_t),
                              TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

    tinygltf::Accessor acc;
    acc.bufferView    = bvIdx;
    acc.byteOffset    = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    acc.count         = static_cast<int>(indices.size());
    acc.type          = TINYGLTF_TYPE_SCALAR;
    model.accessors.push_back(std::move(acc));
    return static_cast<int>(model.accessors.size()) - 1;
}

// ---------------------------------------------------------------------------
// Texture helpers
// ---------------------------------------------------------------------------

static std::unordered_map<std::string, int>
buildTextures(tinygltf::Model& model,
              const std::map<std::string, Mc3Texture>& textures,
              const std::filesystem::path& basePath,
              bool embedImages)
{
    std::unordered_map<std::string, int> texIdx;
    for (const auto& [name, tex] : textures) {
        tinygltf::Sampler samp;
        samp.wrapS = (tex.wrapU == "clamp") ? TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE
                                             : TINYGLTF_TEXTURE_WRAP_REPEAT;
        samp.wrapT = (tex.wrapV == "clamp") ? TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE
                                             : TINYGLTF_TEXTURE_WRAP_REPEAT;
        samp.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
        samp.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;

        int sampIdx = static_cast<int>(model.samplers.size());
        model.samplers.push_back(std::move(samp));

        tinygltf::Image img;
        img.name = tex.name;

        if (embedImages) {
            // GLB: load raw bytes so tinygltf can embed them as a data URI.
            // Without pixel data, tinygltf's embed path silently strips the
            // directory prefix and emits a bare filename URI that Blender can't find.
            std::filesystem::path imgPath = basePath / tex.uri;
            std::ifstream ifs(imgPath, std::ios::binary);
            if (ifs) {
                img.image = std::vector<unsigned char>(
                    std::istreambuf_iterator<char>(ifs),
                    std::istreambuf_iterator<char>());
                img.as_is    = true;          // already-encoded PNG bytes
                img.mimeType = "image/png";   // drives tinygltf's ext detection
            } else {
                img.uri = tex.uri;
                std::cerr << "[mc3togltf] Warning: texture not found for embedding: "
                          << imgPath << "\n";
            }
        } else {
            // GLTF: keep relative URI so tools can load textures from next to the file
            img.uri = tex.uri;
        }

        int imgIdx = static_cast<int>(model.images.size());
        model.images.push_back(std::move(img));

        tinygltf::Texture gtex;
        gtex.name    = tex.name;
        gtex.source  = imgIdx;
        gtex.sampler = sampIdx;

        int tIdx = static_cast<int>(model.textures.size());
        model.textures.push_back(std::move(gtex));

        texIdx[name] = tIdx;
    }
    return texIdx;
}

// ---------------------------------------------------------------------------
// Material helpers
// ---------------------------------------------------------------------------

static int buildMaterial(tinygltf::Model& model,
                         const Mc3Material& mat,
                         const std::unordered_map<std::string, int>& texIdx)
{
    tinygltf::Material m;
    m.name = mat.name;

    auto& pbr = m.pbrMetallicRoughness;
    pbr.baseColorFactor = {
        mat.baseColor[0], mat.baseColor[1], mat.baseColor[2], mat.baseColor[3]
    };
    pbr.metallicFactor  = mat.metallic;
    pbr.roughnessFactor = mat.roughness;

    if (!mat.baseColorTexture.empty()) {
        auto it = texIdx.find(mat.baseColorTexture);
        if (it != texIdx.end()) {
            pbr.baseColorTexture.index    = it->second;
            pbr.baseColorTexture.texCoord = 0;
        }
    }
    if (!mat.metallicRoughnessTexture.empty()) {
        auto it = texIdx.find(mat.metallicRoughnessTexture);
        if (it != texIdx.end()) {
            pbr.metallicRoughnessTexture.index    = it->second;
            pbr.metallicRoughnessTexture.texCoord = 0;
        }
    }
    if (!mat.normalTexture.empty()) {
        auto it = texIdx.find(mat.normalTexture);
        if (it != texIdx.end()) {
            m.normalTexture.index    = it->second;
            m.normalTexture.texCoord = 0;
            m.normalTexture.scale    = mat.normalScale;
        }
    }
    if (!mat.occlusionTexture.empty()) {
        auto it = texIdx.find(mat.occlusionTexture);
        if (it != texIdx.end()) {
            m.occlusionTexture.index    = it->second;
            m.occlusionTexture.texCoord = 0;
            m.occlusionTexture.strength = mat.occlusionStrength;
        }
    }
    if (!mat.emissiveTexture.empty()) {
        auto it = texIdx.find(mat.emissiveTexture);
        if (it != texIdx.end()) {
            m.emissiveTexture.index    = it->second;
            m.emissiveTexture.texCoord = 0;
        }
    }

    m.emissiveFactor = {
        mat.emissiveColor[0], mat.emissiveColor[1], mat.emissiveColor[2]
    };

    m.doubleSided = mat.doubleSided;

    std::string am = mat.alphaMode;
    std::transform(am.begin(), am.end(), am.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    if (am == "BLEND")      m.alphaMode = "BLEND";
    else if (am == "MASK")  { m.alphaMode = "MASK"; m.alphaCutoff = mat.alphaCutoff; }
    else                    m.alphaMode = "OPAQUE";

    model.materials.push_back(std::move(m));
    return static_cast<int>(model.materials.size()) - 1;
}

// ---------------------------------------------------------------------------
// Mesh builder (from Mc3Object)
// ---------------------------------------------------------------------------

static int buildMesh(ExportCtx& ctx,
                     const Mc3Object& obj,
                     int materialIdx)
{
    tinygltf::Model& model = ctx.model;
    MeshData md;

    if (obj.type == ObjectType::Mesh && !obj.meshSource.empty()) {
        try {
            md = loadObjMesh(ctx.basePath, obj.meshSource);
        } catch (const std::exception& e) {
            std::cerr << "Warning: " << e.what() << '\n';
            ctx.stats.warnings++;
            return -1;
        }
    } else if (obj.extrude.has_value()) {
        md = buildExtrude(*obj.extrude);
    } else if (obj.primitive.has_value()) {
        md = buildPrimitive(*obj.primitive);
    }

    if (obj.deform.has_value()) {
        md.applyScale(obj.deform->scale[0],
                      obj.deform->scale[1],
                      obj.deform->scale[2]);
    }

    // Apply unit scale to geometry positions
    if (ctx.unitScale != 1.0f)
        md.applyScale(ctx.unitScale, ctx.unitScale, ctx.unitScale);

    if (md.empty()) return -1;

    int posAcc  = addAccessorVec3(model, md.positions, /*calcBounds=*/true);
    int normAcc = addAccessorVec3(model, md.normals);
    int uvAcc   = addAccessorVec2(model, md.texcoords);
    int idxAcc  = addAccessorIndices(model, md.indices);

    tinygltf::Primitive prim;
    prim.attributes["POSITION"]   = posAcc;
    prim.attributes["NORMAL"]     = normAcc;
    prim.attributes["TEXCOORD_0"] = uvAcc;
    prim.indices = idxAcc;
    prim.mode    = TINYGLTF_MODE_TRIANGLES;
    if (materialIdx >= 0) prim.material = materialIdx;

    tinygltf::Mesh mesh;
    mesh.name = obj.name;
    mesh.primitives.push_back(std::move(prim));
    model.meshes.push_back(std::move(mesh));
    return static_cast<int>(model.meshes.size()) - 1;
}

// ---------------------------------------------------------------------------
// Add a pre-built MeshData to the glTF model.  Applies ctx.unitScale.
// Returns the glTF mesh index, or -1 if md is empty.
// ---------------------------------------------------------------------------

static int addMeshDataToGltf(ExportCtx& ctx, MeshData md,
                              const std::string& name, int materialIdx)
{
    if (md.empty()) return -1;
    if (ctx.unitScale != 1.0f)
        md.applyScale(ctx.unitScale, ctx.unitScale, ctx.unitScale);

    tinygltf::Model& model = ctx.model;
    int posAcc  = addAccessorVec3(model, md.positions, /*calcBounds=*/true);
    int normAcc = addAccessorVec3(model, md.normals);
    int idxAcc  = addAccessorIndices(model, md.indices);

    tinygltf::Primitive prim;
    prim.attributes["POSITION"] = posAcc;
    prim.attributes["NORMAL"]   = normAcc;
    if (!md.texcoords.empty()) {
        int uvAcc = addAccessorVec2(model, md.texcoords);
        prim.attributes["TEXCOORD_0"] = uvAcc;
    }
    prim.indices = idxAcc;
    prim.mode    = TINYGLTF_MODE_TRIANGLES;
    if (materialIdx >= 0) prim.material = materialIdx;

    tinygltf::Mesh mesh;
    mesh.name = name;
    mesh.primitives.push_back(std::move(prim));
    model.meshes.push_back(std::move(mesh));
    return static_cast<int>(model.meshes.size()) - 1;
}

// ---------------------------------------------------------------------------
// Node building (recursive)
// ---------------------------------------------------------------------------

static int buildNode(ExportCtx& ctx, const Mc3Object& obj)
{
    ctx.stats.objectsProcessed++;

    // Invisible objects (and their entire subtree) are skipped
    if (!obj.visible) return -1;

    tinygltf::Node node;
    node.name = obj.name;

    const auto& t = obj.transform;
    const bool hasPivot = (t.pivot[0] != 0.0f || t.pivot[1] != 0.0f || t.pivot[2] != 0.0f);

    // --- Transform ---
    // With a non-zero pivot, the rotation/scale centre is at (position + pivot) in parent space.
    // We represent this as:
    //   outer node: T(position + pivot), R, S
    //   inner "origin" child: T(-pivot)       ← mesh and logical children live here
    if (!hasPivot) {
        if (t.position[0] != 0.0f || t.position[1] != 0.0f || t.position[2] != 0.0f)
            node.translation = {t.position[0], t.position[1], t.position[2]};
    } else {
        node.translation = {
            t.position[0] + t.pivot[0],
            t.position[1] + t.pivot[1],
            t.position[2] + t.pivot[2]
        };
    }

    if (t.rotation[0] != 0.0f || t.rotation[1] != 0.0f || t.rotation[2] != 0.0f) {
        auto q = eulerXYZToQuat(t.rotation[0], t.rotation[1], t.rotation[2]);
        node.rotation = {q[0], q[1], q[2], q[3]};
    }

    if (t.scale[0] != 1.0f || t.scale[1] != 1.0f || t.scale[2] != 1.0f)
        node.scale = {t.scale[0], t.scale[1], t.scale[2]};

    // --- Material (materialOverride takes priority) ---
    int matIdx = -1;
    const std::string& matName = !obj.materialOverride.empty() ? obj.materialOverride : obj.material;
    if (!matName.empty()) {
        auto it = ctx.matNameToIdx.find(matName);
        if (it != ctx.matNameToIdx.end()) matIdx = it->second;
    }

    // --- Geometry ---
    // Nodes that own direct geometry: all types with a primitive or extrude,
    // plus Instance (resolved via definitions).
    int directMesh = -1;

    if (obj.type == ObjectType::Instance && !obj.definition.empty()) {
        auto it = ctx.definitions.find(obj.definition);
        if (it != ctx.definitions.end() && it->second) {
            const Mc3Object& defObj = *it->second;

            int effectiveMat = matIdx >= 0 ? matIdx : [&]{
                auto jt = ctx.matNameToIdx.find(defObj.material);
                return jt != ctx.matNameToIdx.end() ? jt->second : -1;
            }();

            // Reuse cached mesh if same definition+material+deform was already built
            auto cacheKey = buildDefCacheKey(obj.definition, effectiveMat, obj.deform);
            auto cacheIt  = ctx.defMeshCache.find(cacheKey);
            if (cacheIt != ctx.defMeshCache.end()) {
                directMesh = cacheIt->second;
                ctx.stats.reusedMeshRefs++;
            } else {
                Mc3Object tmp = defObj;
                if (matIdx >= 0) tmp.material = matName;
                if (obj.deform.has_value()) tmp.deform = obj.deform;
                directMesh = buildMesh(ctx, tmp, effectiveMat);
                ctx.defMeshCache[cacheKey] = directMesh;
            }

            // Recurse into definition's children
            for (const auto& child : defObj.children) {
                if (!child) continue;
                int ci = buildNode(ctx, *child);
                if (ci >= 0) node.children.push_back(ci);
            }
        } else {
            std::cerr << "Warning: instance references unknown definition '"
                      << obj.definition << "'\n";
            ctx.stats.warnings++;
        }
    } else if (obj.primitive.has_value() || obj.extrude.has_value() ||
               (obj.type == ObjectType::Mesh && !obj.meshSource.empty())) {
        std::string geomKey = buildGeomCacheKey(obj, matIdx);
        auto gIt = ctx.geomMeshCache.find(geomKey);
        if (gIt != ctx.geomMeshCache.end()) {
            directMesh = gIt->second;
            if (directMesh >= 0) ctx.stats.reusedMeshRefs++;
        } else {
            directMesh = buildMesh(ctx, obj, matIdx);
            ctx.geomMeshCache[geomKey] = directMesh;
            if (obj.type == ObjectType::Mesh) ctx.stats.objMeshesLoaded++;
        }
    }

    // --- CSG nodes: real boolean evaluation via Manifold ---
    bool csgEvaluated = false;
    if (obj.type == ObjectType::Union ||
        obj.type == ObjectType::Difference ||
        obj.type == ObjectType::Intersection) {

        const char* op = (obj.type == ObjectType::Union)      ? "union"
                       : (obj.type == ObjectType::Difference) ? "difference"
                       :                                        "intersection";

        if (!ctx.allowApproximateCSG) {
            // Real CSG: evaluate with Manifold, produce a single merged mesh.
            // Throws on error so the export fails loudly rather than silently wrong.
            MeshData csgData = evaluateCsgNode(obj, ctx.definitions);
            if (!csgData.empty())
                directMesh = addMeshDataToGltf(ctx, std::move(csgData), obj.name, matIdx);
            ctx.stats.csgMeshesEvaluated++;
            csgEvaluated = true;   // skip children — they are baked into the mesh
        } else {
            // Approximate mode (--allow-approximate-csg / editor checkbox):
            // export children as separate meshes.  Geometrically incorrect.
            std::cerr << "Warning: mc3togltf: <" << op << "> node '"
                      << (obj.name.empty() ? "(unnamed)" : obj.name)
                      << "' — approximate CSG mode; children exported as separate meshes.\n";
            ctx.stats.warnings++;
        }
    }

    // --- Logical children (skip when real CSG was evaluated) ---
    if (!csgEvaluated && obj.type != ObjectType::Instance) {
        for (const auto& child : obj.children) {
            if (!child) continue;
            int ci = buildNode(ctx, *child);
            if (ci >= 0) node.children.push_back(ci);
        }
    }

    // --- Apply pivot: wrap geometry/children in an inner offset node ---
    if (hasPivot) {
        tinygltf::Node originNode;
        originNode.name        = obj.name + "_origin";
        originNode.translation = {
            -t.pivot[0] * ctx.unitScale,
            -t.pivot[1] * ctx.unitScale,
            -t.pivot[2] * ctx.unitScale
        };
        originNode.mesh        = directMesh;
        originNode.children    = node.children;
        node.children.clear();

        ctx.model.nodes.push_back(std::move(originNode));
        int originIdx = static_cast<int>(ctx.model.nodes.size()) - 1;
        node.children.push_back(originIdx);
    } else {
        node.mesh = directMesh;
    }

    // Tags, collision, and mc3_type → node extras (useful for game-engine import)
    {
        tinygltf::Value::Object extras;
        bool hasExtras = false;

        if (obj.type == ObjectType::Area) {
            extras["mc3_type"] = tinygltf::Value(std::string("area"));
            hasExtras = true;
        }

        if (!obj.tags.empty()) {
            tinygltf::Value::Array tagsArr;
            for (const auto& tag : obj.tags)
                tagsArr.push_back(tinygltf::Value(tag));
            extras["tags"] = tinygltf::Value(tagsArr);
            hasExtras = true;
        }
        if (!obj.collision.empty() && obj.collision != "none") {
            extras["collision"] = tinygltf::Value(obj.collision);
            hasExtras = true;
        }
        if (hasExtras)
            node.extras = tinygltf::Value(extras);
    }

    // Apply unit scale to node translation
    if (ctx.unitScale != 1.0f && !node.translation.empty()) {
        node.translation[0] *= ctx.unitScale;
        node.translation[1] *= ctx.unitScale;
        node.translation[2] *= ctx.unitScale;
    }

    ctx.model.nodes.push_back(std::move(node));
    return static_cast<int>(ctx.model.nodes.size()) - 1;
}

// ---------------------------------------------------------------------------
// Lights (KHR_lights_punctual)
// ---------------------------------------------------------------------------

static void addLights(tinygltf::Model& model,
                      const std::vector<Mc3Light>& lights,
                      std::vector<int>& outLightNodeIndices)
{
    if (lights.empty()) return;

    model.extensionsUsed.push_back("KHR_lights_punctual");

    tinygltf::Value::Array lightsArray;

    for (const auto& light : lights) {
        if (light.type == LightType::Ambient) continue; // no ambient in glTF 2.0

        tinygltf::Value::Object lo;
        lo["name"]  = tinygltf::Value(light.name);
        lo["color"] = tinygltf::Value(tinygltf::Value::Array{
            tinygltf::Value(static_cast<double>(light.color[0])),
            tinygltf::Value(static_cast<double>(light.color[1])),
            tinygltf::Value(static_cast<double>(light.color[2]))
        });
        lo["intensity"] = tinygltf::Value(static_cast<double>(light.brightness));

        std::string typeStr;
        switch (light.type) {
            case LightType::Directional: typeStr = "directional"; break;
            case LightType::Spot:        typeStr = "spot";        break;
            case LightType::Point:       typeStr = "point";       break;
            default:                     typeStr = "directional"; break;
        }
        lo["type"] = tinygltf::Value(typeStr);

        if (light.range > 0.0f)
            lo["range"] = tinygltf::Value(static_cast<double>(light.range));

        if (light.type == LightType::Spot) {
            double halfAngle  = light.angle * std::numbers::pi / 360.0;
            double innerAngle = halfAngle * (1.0 - std::clamp(light.falloff, 0.0f, 1.0f));
            tinygltf::Value::Object spot;
            spot["outerConeAngle"] = tinygltf::Value(halfAngle);
            spot["innerConeAngle"] = tinygltf::Value(innerAngle);
            lo["spot"] = tinygltf::Value(spot);
        }

        int lightIdx = static_cast<int>(lightsArray.size());
        lightsArray.push_back(tinygltf::Value(lo));

        tinygltf::Node lnode;
        lnode.name = light.name;

        if (light.type == LightType::Directional || light.type == LightType::Spot) {
            auto q = directionToQuat(light.direction[0],
                                     light.direction[1],
                                     light.direction[2]);
            lnode.rotation = {q[0], q[1], q[2], q[3]};
        } else {
            lnode.translation = {
                static_cast<double>(light.position[0]),
                static_cast<double>(light.position[1]),
                static_cast<double>(light.position[2])
            };
        }

        tinygltf::Value::Object nodeExt;
        nodeExt["light"] = tinygltf::Value(lightIdx);
        lnode.extensions["KHR_lights_punctual"] = tinygltf::Value(nodeExt);

        if (light.castShadows) {
            tinygltf::Value::Object extras;
            extras["castShadows"] = tinygltf::Value(true);
            lnode.extras = tinygltf::Value(extras);
        }

        int nodeIdx = static_cast<int>(model.nodes.size());
        model.nodes.push_back(std::move(lnode));
        outLightNodeIndices.push_back(nodeIdx);
    }

    tinygltf::Value::Object extObj;
    extObj["lights"] = tinygltf::Value(lightsArray);
    model.extensions["KHR_lights_punctual"] = tinygltf::Value(extObj);
}

// ---------------------------------------------------------------------------
// Cameras
// ---------------------------------------------------------------------------

static void addCameraNodes(tinygltf::Model& model,
                           const std::vector<Mc3Camera>& cameras,
                           std::vector<int>& outCameraNodeIndices)
{
    for (const auto& cam : cameras) {
        tinygltf::Camera gcam;
        gcam.name = cam.name;

        if (cam.type == CameraType::Perspective) {
            gcam.type = "perspective";
            gcam.perspective.yfov        = cam.fov * std::numbers::pi / 180.0;
            gcam.perspective.znear       = cam.nearPlane;
            gcam.perspective.zfar        = cam.farPlane;
            gcam.perspective.aspectRatio = 16.0 / 9.0;
        } else {
            gcam.type = "orthographic";
            gcam.orthographic.xmag  = cam.orthoSize;
            gcam.orthographic.ymag  = cam.orthoSize;
            gcam.orthographic.znear = cam.nearPlane;
            gcam.orthographic.zfar  = cam.farPlane;
        }

        int camIdx = static_cast<int>(model.cameras.size());
        model.cameras.push_back(std::move(gcam));

        tinygltf::Node cnode;
        cnode.name   = cam.name;
        cnode.camera = camIdx;
        cnode.translation = {
            static_cast<double>(cam.position[0]),
            static_cast<double>(cam.position[1]),
            static_cast<double>(cam.position[2])
        };

        if (cam.rotation.has_value()) {
            const auto& r = *cam.rotation;
            auto q = eulerXYZToQuat(r[0], r[1], r[2]);
            cnode.rotation = {q[0], q[1], q[2], q[3]};
        } else {
            float dx = cam.target[0] - cam.position[0];
            float dy = cam.target[1] - cam.position[1];
            float dz = cam.target[2] - cam.position[2];
            auto q = directionToQuat(dx, dy, dz);
            cnode.rotation = {q[0], q[1], q[2], q[3]};
        }

        int nodeIdx = static_cast<int>(model.nodes.size());
        model.nodes.push_back(std::move(cnode));
        outCameraNodeIndices.push_back(nodeIdx);
    }
}

// ---------------------------------------------------------------------------
// Environment → scene extras
// ---------------------------------------------------------------------------

static void applyEnvironment(tinygltf::Scene& scene,
                              const std::optional<Mc3Environment>& env)
{
    if (!env.has_value()) return;

    tinygltf::Value::Object extras;

    extras["backgroundColor"] = tinygltf::Value(tinygltf::Value::Array{
        tinygltf::Value(static_cast<double>(env->backgroundColor[0])),
        tinygltf::Value(static_cast<double>(env->backgroundColor[1])),
        tinygltf::Value(static_cast<double>(env->backgroundColor[2]))
    });

    if (!env->backgroundTexture.empty())
        extras["backgroundTexture"] = tinygltf::Value(env->backgroundTexture);

    if (env->fog.has_value()) {
        const auto& fog = *env->fog;
        tinygltf::Value::Object fogObj;
        fogObj["mode"]    = tinygltf::Value(fog.mode == FogMode::Linear ? "linear" : "exponential");
        fogObj["color"]   = tinygltf::Value(tinygltf::Value::Array{
            tinygltf::Value(static_cast<double>(fog.color[0])),
            tinygltf::Value(static_cast<double>(fog.color[1])),
            tinygltf::Value(static_cast<double>(fog.color[2]))
        });
        fogObj["start"]   = tinygltf::Value(static_cast<double>(fog.start));
        fogObj["end"]     = tinygltf::Value(static_cast<double>(fog.end));
        fogObj["density"] = tinygltf::Value(static_cast<double>(fog.density));
        extras["fog"] = tinygltf::Value(fogObj);
    }

    scene.extras = tinygltf::Value(extras);
}

// ---------------------------------------------------------------------------
// Animation export
// ---------------------------------------------------------------------------

static void collectBaseTransforms(
    const std::vector<std::shared_ptr<Mc3Object>>& objects,
    std::unordered_map<std::string, Mc3Transform>& out)
{
    for (const auto& obj : objects) {
        if (!obj) continue;
        if (!obj->name.empty()) out[obj->name] = obj->transform;
        collectBaseTransforms(obj->children, out);
    }
}

static void exportAnimations(
    tinygltf::Model& model,
    const std::map<std::string, Mc3Action>& actions,
    const std::unordered_map<std::string, int>& nodeNameMap,
    const std::unordered_map<std::string, Mc3Transform>& baseTransforms,
    float unitScale)
{
    if (actions.empty()) return;

    for (const auto& [actionName, action] : actions) {
        // Group per-scalar mc3 channels by (target object, glTF path).
        struct PathGroup {
            std::string path;
            const Mc3Channel* ch[3]{nullptr, nullptr, nullptr};
        };
        std::map<std::string, std::map<std::string, PathGroup>> groups;

        for (const auto& ch : action.channels) {
            std::string path;
            int comp = -1;
            switch (ch.property) {
                case AnimatedProperty::PositionX: path = "translation"; comp = 0; break;
                case AnimatedProperty::PositionY: path = "translation"; comp = 1; break;
                case AnimatedProperty::PositionZ: path = "translation"; comp = 2; break;
                case AnimatedProperty::RotationX: path = "rotation";    comp = 0; break;
                case AnimatedProperty::RotationY: path = "rotation";    comp = 1; break;
                case AnimatedProperty::RotationZ: path = "rotation";    comp = 2; break;
                case AnimatedProperty::ScaleX:    path = "scale";       comp = 0; break;
                case AnimatedProperty::ScaleY:    path = "scale";       comp = 1; break;
                case AnimatedProperty::ScaleZ:    path = "scale";       comp = 2; break;
                default:
                    std::cerr << "Warning: mc3togltf: action '" << actionName
                              << "': channel property '" << animatedPropertyName(ch.property)
                              << "' on target '" << ch.targetObject
                              << "' has no glTF node-transform equivalent — channel skipped.\n";
                    continue;
            }
            auto& pg = groups[ch.targetObject][path];
            pg.path = path;
            pg.ch[comp] = &ch;
        }

        if (groups.empty()) continue;

        tinygltf::Animation anim;
        anim.name = action.name;

        for (const auto& [objName, pathMap] : groups) {
            auto nodeIt = nodeNameMap.find(objName);
            if (nodeIt == nodeNameMap.end()) continue;
            int nodeIdx = nodeIt->second;

            // Fallback base transform for non-animated components.
            Mc3Transform baseT;
            baseT.scale = {1.0f, 1.0f, 1.0f};
            auto btIt = baseTransforms.find(objName);
            if (btIt != baseTransforms.end()) baseT = btIt->second;

            for (const auto& [path, pg] : pathMap) {
                // Collect union of keyframe times from all present component channels.
                std::set<float> timeSet;
                bool hasCubic = false;
                bool allStep  = true;

                for (int i = 0; i < 3; ++i) {
                    if (!pg.ch[i]) continue;
                    for (const auto& kf : pg.ch[i]->keyframes) {
                        timeSet.insert(kf.time);
                        if (kf.interpolation == Interpolation::CubicBezier) hasCubic = true;
                        if (kf.interpolation != Interpolation::Step)        allStep  = false;
                    }
                }
                if (timeSet.empty()) continue;

                // Dense sampling for cubic bezier to preserve curve shape in LINEAR glTF output.
                if (hasCubic) {
                    float minT = *timeSet.begin(), maxT = *timeSet.rbegin();
                    for (float t = minT; t <= maxT + 1e-5f; t += 1.0f / 30.0f)
                        timeSet.insert(t);
                }

                std::vector<float> times(timeSet.begin(), timeSet.end());
                std::string interp = allStep ? "STEP" : "LINEAR";

                // Base component values for non-animated axes.
                float base[3];
                if (path == "translation") {
                    base[0] = baseT.position[0]; base[1] = baseT.position[1]; base[2] = baseT.position[2];
                } else if (path == "rotation") {
                    base[0] = baseT.rotation[0]; base[1] = baseT.rotation[1]; base[2] = baseT.rotation[2];
                } else {
                    base[0] = baseT.scale[0]; base[1] = baseT.scale[1]; base[2] = baseT.scale[2];
                }

                // Build output value data.
                std::vector<float> valueData;
                if (path == "rotation") {
                    valueData.reserve(times.size() * 4);
                    for (float t : times) {
                        float euler[3];
                        for (int i = 0; i < 3; ++i)
                            euler[i] = pg.ch[i] ? evaluateChannel(*pg.ch[i], t) : base[i];
                        auto q = eulerXYZToQuat(euler[0], euler[1], euler[2]);
                        valueData.push_back(static_cast<float>(q[0]));
                        valueData.push_back(static_cast<float>(q[1]));
                        valueData.push_back(static_cast<float>(q[2]));
                        valueData.push_back(static_cast<float>(q[3]));
                    }
                } else {
                    float tScale = (path == "translation") ? unitScale : 1.0f;
                    valueData.reserve(times.size() * 3);
                    for (float t : times) {
                        for (int i = 0; i < 3; ++i) {
                            float v = pg.ch[i] ? evaluateChannel(*pg.ch[i], t) : base[i];
                            valueData.push_back(v * tScale);
                        }
                    }
                }

                // Time accessor (SCALAR, min/max required by glTF spec).
                {
                    int bv = addBufferView(model, times.data(), times.size() * sizeof(float), 0);
                    tinygltf::Accessor acc;
                    acc.bufferView    = bv;
                    acc.byteOffset    = 0;
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    acc.count         = static_cast<int>(times.size());
                    acc.type          = TINYGLTF_TYPE_SCALAR;
                    acc.minValues     = {static_cast<double>(times.front())};
                    acc.maxValues     = {static_cast<double>(times.back())};
                    model.accessors.push_back(std::move(acc));
                }
                int inputAcc = static_cast<int>(model.accessors.size()) - 1;

                // Value accessor (VEC3 or VEC4).
                {
                    int bv = addBufferView(model, valueData.data(), valueData.size() * sizeof(float), 0);
                    tinygltf::Accessor acc;
                    acc.bufferView    = bv;
                    acc.byteOffset    = 0;
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    if (path == "rotation") {
                        acc.count = static_cast<int>(valueData.size()) / 4;
                        acc.type  = TINYGLTF_TYPE_VEC4;
                    } else {
                        acc.count = static_cast<int>(valueData.size()) / 3;
                        acc.type  = TINYGLTF_TYPE_VEC3;
                    }
                    model.accessors.push_back(std::move(acc));
                }
                int outputAcc = static_cast<int>(model.accessors.size()) - 1;

                tinygltf::AnimationSampler sampler;
                sampler.input         = inputAcc;
                sampler.output        = outputAcc;
                sampler.interpolation = interp;
                int sampIdx = static_cast<int>(anim.samplers.size());
                anim.samplers.push_back(std::move(sampler));

                tinygltf::AnimationChannel chan;
                chan.sampler     = sampIdx;
                chan.target_node = nodeIdx;
                chan.target_path = path;
                anim.channels.push_back(std::move(chan));
            }
        }

        if (!anim.channels.empty())
            model.animations.push_back(std::move(anim));
    }
}

// ---------------------------------------------------------------------------
// GltfExporter::exportDocument
// ---------------------------------------------------------------------------

void GltfExporter::exportDocument(const Mc3Document& doc,
                                   const std::filesystem::path& outputPath,
                                   OutputFormat format)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF writer;

    model.buffers.emplace_back();
    model.buffers[0].name = "buffer0";
    model.asset.version   = "2.0";
    model.asset.generator = "mc3togltf";

    // Store MC3 document metadata in asset.extras
    {
        tinygltf::Value::Object ae;
        if (!doc.model.empty())   ae["mc3_model"]   = tinygltf::Value(doc.model);
        if (!doc.version.empty()) ae["mc3_version"]  = tinygltf::Value(doc.version);
        if (doc.unit != "meter")  ae["mc3_unit"]     = tinygltf::Value(doc.unit);
        if (!ae.empty()) model.asset.extras = tinygltf::Value(ae);
    }

    // Textures (embedImages=true for GLB so images are embedded as data URIs)
    bool embedImagesNow = (format == OutputFormat::GLB);
    auto texIdx = buildTextures(model, doc.textures, doc.sourcePath, embedImagesNow);

    // Materials
    std::unordered_map<std::string, int> matNameToIdx;
    for (const auto& [name, mat] : doc.materials) {
        int idx = buildMaterial(model, mat, texIdx);
        matNameToIdx[name] = idx;
    }

    // Scene
    tinygltf::Scene scene;
    scene.name = doc.model.empty() ? "Scene" : doc.model;

    // Object nodes (recursive)
    ExportCtx ctx{model, matNameToIdx, doc.definitions,
                  unitScaleFactor(doc.unit), doc.sourcePath,
                  allowApproximateCSG, {}, {}, {}};
    for (const auto& objPtr : doc.objects) {
        if (!objPtr) continue;
        int nodeIdx = buildNode(ctx, *objPtr);
        if (nodeIdx >= 0) scene.nodes.push_back(nodeIdx);
    }

    // Lights
    std::vector<int> lightNodes;
    addLights(model, doc.lights, lightNodes);
    for (int i : lightNodes) scene.nodes.push_back(i);

    // Cameras
    std::vector<int> cameraNodes;
    addCameraNodes(model, doc.cameras, cameraNodes);
    for (int i : cameraNodes) scene.nodes.push_back(i);

    // Animations
    if (!doc.actions.empty()) {
        std::unordered_map<std::string, Mc3Transform> baseTransforms;
        collectBaseTransforms(doc.objects, baseTransforms);

        std::unordered_map<std::string, int> nodeNameMap;
        for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
            if (!model.nodes[i].name.empty())
                nodeNameMap[model.nodes[i].name] = i;
        }

        exportAnimations(model, doc.actions, nodeNameMap, baseTransforms, ctx.unitScale);
    }

    // Environment → scene extras
    applyEnvironment(scene, doc.environment);

    model.scenes.push_back(std::move(scene));
    model.defaultScene = 0;

    bool writeBinary = (format == OutputFormat::GLB);
    bool embedImages = writeBinary;
    bool prettyPrint = !writeBinary;

    std::string path = outputPath.string();
    bool ok = writer.WriteGltfSceneToFile(&model, path,
                                           embedImages,
                                           /*embedBuffers=*/writeBinary,
                                           prettyPrint,
                                           writeBinary);
    if (!ok)
        throw std::runtime_error("tinygltf: failed to write " + path);

    // Populate export statistics from accumulated ctx.stats + model aggregate counts.
    stats = ctx.stats;
    stats.gltfNodes    = static_cast<int>(model.nodes.size());
    stats.uniqueMeshes = static_cast<int>(model.meshes.size());
    for (const auto& mesh : model.meshes) {
        for (const auto& prim : mesh.primitives) {
            auto posIt = prim.attributes.find("POSITION");
            if (posIt != prim.attributes.end() &&
                posIt->second >= 0 &&
                posIt->second < static_cast<int>(model.accessors.size()))
                stats.totalVertices += model.accessors[posIt->second].count;
            if (prim.indices >= 0 &&
                prim.indices < static_cast<int>(model.accessors.size()))
                stats.totalTriangles += model.accessors[prim.indices].count / 3;
        }
    }
}

} // namespace mc3togltf
