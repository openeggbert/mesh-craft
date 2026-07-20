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
#include <array>
#include <cctype>
#include <cmath>
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
    bool allowExternalResources{false}; // permit out-of-root texture/mesh paths
    bool rotationIsRadians{false};   // STAB-0691: doc.rotationUnits == "radians"
    std::string eulerOrder{"XYZ"};   // STAB-0691: doc.eulerOrder

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
// the glTF primitive).  Does NOT include node-level transform (handled as TRS,
// so e.g. two boxes at different positions correctly share one mesh — STAB-0254).
//
// STAB-0255: fields folded into the key, by ObjectType (see buildGeomCacheKey()
// body below for the exact serialization):
//   type tag (int, always first) --------------- obj.type
//   Mesh ----------------------------------------- obj.meshSource (the OBJ path)
//   any primitive type (Box/Sphere/Cylinder/…) --- primitiveType, size, radius,
//                                                   height, segments, axis,
//                                                   majorRadius, minorRadius,
//                                                   subdivisionsX, subdivisionsZ
//   Extrude ---------------------------------------- crossSection (type, width,
//                                                   height, radius, innerRadius,
//                                                   sides, segments, customPoints),
//                                                   path (type, length, axis,
//                                                   arcRadius, arcAngle,
//                                                   helixRadius/Height/Turns,
//                                                   points), twist, segments,
//                                                   smooth, caps
//   (any type, if present) ------------------------- deform.scale
//   (always last) ----------------------------------- effective material index
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

    // AUD-024: uv_mapping changes the built MeshData's texcoords (see
    // buildMesh) but was omitted here, so two same-size/same-material boxes
    // that differ only by uv_mapping collided on the same cache key -- the
    // second one silently reused the first's (wrongly, differently-mapped)
    // TEXCOORD_0 instead of getting its own.
    if (obj.uvMapping.has_value()) {
        const auto& uv = *obj.uvMapping;
        k << "|UV" << static_cast<int>(uv.projection) << ','
          << uv.scaleU << ',' << uv.scaleV << ','
          << uv.offsetU << ',' << uv.offsetV << ',' << uv.rotation;
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
static int buildNode(ExportCtx& ctx, const Mc3Object& obj, int depth = 0);

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
            // A non-finite POSITION component makes the accessor min/max — and
            // the whole glTF — spec-invalid. Degenerate parametric geometry
            // (e.g. a helix with radius=0 or turns=0, which divides by zero) can
            // produce NaN vertices from otherwise-finite inputs, so the parser's
            // input sanitization can't catch this; fail loudly here instead of
            // emitting an invalid file the tool reports as "Written:".
            if (!std::isfinite(data[i]) || !std::isfinite(data[i+1]) ||
                !std::isfinite(data[i+2])) {
                throw std::runtime_error(
                    "non-finite vertex position generated during export "
                    "(NaN/Inf) — check for degenerate geometry parameters "
                    "such as a zero-radius/zero-turn helix or zero-scale object");
            }
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

static int addAccessorVec4(tinygltf::Model& model,
                           const std::vector<float>& data)
{
    int bvIdx = addBufferView(model, data.data(),
                              data.size() * sizeof(float),
                              TINYGLTF_TARGET_ARRAY_BUFFER);

    tinygltf::Accessor acc;
    acc.bufferView    = bvIdx;
    acc.byteOffset    = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.count         = static_cast<int>(data.size() / 4);
    acc.type          = TINYGLTF_TYPE_VEC4;
    model.accessors.push_back(std::move(acc));
    return static_cast<int>(model.accessors.size()) - 1;
}

// STAB-0664: TANGENT attribute for normal-mapped meshes. Not full MikkTSpace
// (angle/area-weighted contributions with feature-vertex splitting) -- uses
// the standard per-triangle-tangent-then-per-vertex-average-then-Gram-
// Schmidt-orthogonalize algorithm (the same approach three.js's own
// BufferGeometry.computeTangents() uses), which produces correct, glTF-
// TANGENT-attribute-compatible results for real-time normal mapping without
// requiring a full MikkTSpace reference-implementation port. Returns a flat
// VEC4 array (xyz tangent + w handedness sign), or empty if positions/
// normals/texcoords/indices are inconsistent (caller should skip TANGENT).
static std::vector<float> computeTangents(const std::vector<float>& positions,
                                          const std::vector<float>& normals,
                                          const std::vector<float>& texcoords,
                                          const std::vector<uint32_t>& indices)
{
    size_t vertCount = positions.size() / 3;
    if (vertCount == 0 || normals.size() != vertCount * 3 ||
        texcoords.size() != vertCount * 2 || indices.size() < 3)
        return {};

    std::vector<std::array<float,3>> tan1(vertCount, {0,0,0});
    std::vector<std::array<float,3>> tan2(vertCount, {0,0,0});

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i], i1 = indices[i+1], i2 = indices[i+2];
        if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount) continue;

        auto P = [&](uint32_t v) { return std::array<float,3>{positions[v*3], positions[v*3+1], positions[v*3+2]}; };
        auto UV = [&](uint32_t v) { return std::array<float,2>{texcoords[v*2], texcoords[v*2+1]}; };
        auto p0 = P(i0), p1 = P(i1), p2 = P(i2);
        auto uv0 = UV(i0), uv1 = UV(i1), uv2 = UV(i2);

        float e1x = p1[0]-p0[0], e1y = p1[1]-p0[1], e1z = p1[2]-p0[2];
        float e2x = p2[0]-p0[0], e2y = p2[1]-p0[1], e2z = p2[2]-p0[2];
        float du1 = uv1[0]-uv0[0], dv1 = uv1[1]-uv0[1];
        float du2 = uv2[0]-uv0[0], dv2 = uv2[1]-uv0[1];

        float denom = du1 * dv2 - du2 * dv1;
        if (std::abs(denom) < 1e-12f) continue;  // degenerate UV triangle -- skip
        float r = 1.0f / denom;

        std::array<float,3> sdir{ (dv2*e1x - dv1*e2x) * r, (dv2*e1y - dv1*e2y) * r, (dv2*e1z - dv1*e2z) * r };
        std::array<float,3> tdir{ (du1*e2x - du2*e1x) * r, (du1*e2y - du2*e1y) * r, (du1*e2z - du2*e1z) * r };

        for (uint32_t v : {i0, i1, i2}) {
            tan1[v][0] += sdir[0]; tan1[v][1] += sdir[1]; tan1[v][2] += sdir[2];
            tan2[v][0] += tdir[0]; tan2[v][1] += tdir[1]; tan2[v][2] += tdir[2];
        }
    }

    std::vector<float> out(vertCount * 4, 0.0f);
    for (size_t v = 0; v < vertCount; ++v) {
        std::array<float,3> n{normals[v*3], normals[v*3+1], normals[v*3+2]};
        std::array<float,3> t = tan1[v];

        // Gram-Schmidt orthogonalize against the vertex normal.
        float ndott = n[0]*t[0] + n[1]*t[1] + n[2]*t[2];
        std::array<float,3> tOrtho{ t[0]-n[0]*ndott, t[1]-n[1]*ndott, t[2]-n[2]*ndott };
        float len = std::sqrt(tOrtho[0]*tOrtho[0] + tOrtho[1]*tOrtho[1] + tOrtho[2]*tOrtho[2]);

        std::array<float,3> tFinal;
        if (len > 1e-8f) {
            tFinal = { tOrtho[0]/len, tOrtho[1]/len, tOrtho[2]/len };
        } else {
            // Degenerate (e.g. zero UV area at this vertex, or tangent
            // exactly parallel to normal) -- fall back to an arbitrary
            // vector perpendicular to the normal, so TANGENT is still a
            // valid unit vector rather than NaN/zero.
            std::array<float,3> arbitrary = (std::abs(n[0]) < 0.9f) ? std::array<float,3>{1,0,0} : std::array<float,3>{0,1,0};
            float ax = n[1]*arbitrary[2]-n[2]*arbitrary[1];
            float ay = n[2]*arbitrary[0]-n[0]*arbitrary[2];
            float az = n[0]*arbitrary[1]-n[1]*arbitrary[0];
            float alen = std::sqrt(ax*ax+ay*ay+az*az);
            tFinal = alen > 1e-8f ? std::array<float,3>{ax/alen, ay/alen, az/alen} : std::array<float,3>{1,0,0};
        }

        // Handedness: w = +1 if (N x T) points the same way as the
        // accumulated bitangent, -1 otherwise (standard glTF/OpenGL
        // convention for reconstructing the bitangent as cross(N,T)*w).
        float cx = n[1]*tFinal[2]-n[2]*tFinal[1];
        float cy = n[2]*tFinal[0]-n[0]*tFinal[2];
        float cz = n[0]*tFinal[1]-n[1]*tFinal[0];
        float handedness = (cx*tan2[v][0] + cy*tan2[v][1] + cz*tan2[v][2]) < 0.0f ? -1.0f : 1.0f;

        out[v*4]   = tFinal[0];
        out[v*4+1] = tFinal[1];
        out[v*4+2] = tFinal[2];
        out[v*4+3] = handedness;
    }
    return out;
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

// STAB-0676: detect the real image format from magic bytes rather than
// assuming PNG for every embedded texture -- an embedded JPEG previously
// got mistagged as image/png, which spec-compliant loaders fail to decode
// (JPEG bytes parsed as PNG). glTF core spec only mandates PNG/JPEG support.
static const char* detectImageMimeType(const std::vector<unsigned char>& bytes, int& warningCount) {
    static constexpr unsigned char kPngMagic[]  = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    static constexpr unsigned char kJpegMagic[] = {0xFF, 0xD8, 0xFF};
    if (bytes.size() >= sizeof(kPngMagic) &&
        std::equal(std::begin(kPngMagic), std::end(kPngMagic), bytes.begin()))
        return "image/png";
    if (bytes.size() >= sizeof(kJpegMagic) &&
        std::equal(std::begin(kJpegMagic), std::end(kJpegMagic), bytes.begin()))
        return "image/jpeg";
    // Unknown format: default to PNG (prior behavior) rather than emitting
    // no mimeType at all, but this is now a documented fallback, not silent.
    std::cerr << "[mc3togltf] Warning: could not detect image format from "
                 "magic bytes, defaulting to image/png (may be wrong).\n";
    ++warningCount;
    return "image/png";
}

// AUD-026: `warningCount` (ExportCtx::stats.warnings at the call site) must
// be incremented at every "Warning:" print in this file -- otherwise
// `--stats` reports a "Warnings" total that undercounts what was actually
// printed to stderr, misrepresenting export health. These free functions
// don't take ExportCtx (most are reused/testable without one), so the
// count is threaded through as a plain reference instead.
static std::unordered_map<std::string, int>
buildTextures(tinygltf::Model& model,
              const std::map<std::string, Mc3Texture>& textures,
              const std::filesystem::path& basePath,
              const std::filesystem::path& outDir,
              bool embedImages,
              bool allowExternalResources,
              int& warningCount)
{
    std::unordered_map<std::string, int> texIdx;
    for (const auto& [name, tex] : textures) {
        // Untrusted-input containment: refuse a texture URI that escapes the
        // document root (see assertResourceAllowed) unless explicitly allowed.
        assertResourceAllowed(basePath, tex.uri, allowExternalResources, "texture uri");
        auto wrapMode = [](const std::string& w) {
            if (w == "clamp")  return TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
            if (w == "mirror") return TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT;
            return TINYGLTF_TEXTURE_WRAP_REPEAT;
        };
        tinygltf::Sampler samp;
        samp.wrapS = wrapMode(tex.wrapU);
        samp.wrapT = wrapMode(tex.wrapV);
        bool nearest = (tex.filter == "nearest");
        // SYS-W14-22: minFilter used to unconditionally request a mipmapped
        // filter regardless of tex.mipMaps -- a texture explicitly authored
        // with mipMaps="false" (e.g. pixel-art/UI textures where mip
        // blending is undesirable) still told glTF-conformant viewers to
        // generate and sample a mip chain for it. Honor the flag: only
        // request the *_MIPMAP_* variant when mipMaps is actually true.
        if (tex.mipMaps) {
            samp.minFilter = nearest ? TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST
                                      : TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
        } else {
            samp.minFilter = nearest ? TINYGLTF_TEXTURE_FILTER_NEAREST
                                      : TINYGLTF_TEXTURE_FILTER_LINEAR;
        }
        samp.magFilter = nearest ? TINYGLTF_TEXTURE_FILTER_NEAREST
                                  : TINYGLTF_TEXTURE_FILTER_LINEAR;

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
                img.as_is    = true;                          // already-encoded bytes
                img.mimeType = detectImageMimeType(img.image, warningCount); // STAB-0676
            } else {
                img.uri = tex.uri;
                std::cerr << "[mc3togltf] Warning: texture not found for embedding: "
                          << imgPath << "\n";
                ++warningCount;
            }
        } else {
            // GLTF: STAB-0677 -- tex.uri is relative to basePath (the
            // source .mc3.xml's directory), not necessarily to outDir
            // (the .gltf's own directory). If exporting to a different
            // directory (e.g. into dist/), the raw source-relative URI
            // would be wrong/unresolvable for anything loading the .gltf
            // from its own location. Re-relativize against outDir; if
            // that's not possible (e.g. different drive/root on Windows),
            // fall back to an absolute path rather than a silently-broken
            // relative one.
            std::error_code ec;
            std::filesystem::path absTexPath =
                std::filesystem::weakly_canonical(basePath / tex.uri, ec);
            if (ec) absTexPath = basePath / tex.uri;
            std::filesystem::path rebased =
                std::filesystem::relative(absTexPath, outDir, ec);
            img.uri = (!ec && !rebased.empty())
                          ? rebased.generic_string()
                          : absTexPath.generic_string();
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
                         const std::unordered_map<std::string, int>& texIdx,
                         const std::map<std::string, Mc3SvgTexture>& svgTextures,
                         const std::map<std::string, Mc3Texture>& textures,
                         int& warningCount)
{
    tinygltf::Material m;
    m.name = mat.name;

    // STAB-0440: SVG textures aren't rasterized (no code path reads
    // doc.svgTextures at all), so a material referencing one resolves to
    // nothing in texIdx. Name the reason instead of silently dropping it.
    auto warnIfUnresolvedSvg = [&](const std::string& texRef, const char* slot) {
        if (svgTextures.count(texRef)) {
            std::cerr << "Warning: material '" << mat.name << "' references SVG texture '"
                      << texRef << "' as " << slot
                      << " — SVG rasterization is not implemented, texture skipped\n";
            ++warningCount;
        }
    };

    // SYS-W14-23: glTF 2.0 requires baseColorTexture/emissiveTexture to be
    // sRGB-encoded and normalTexture/metallicRoughnessTexture/
    // occlusionTexture to be linear (non-color) data -- a fixed, spec-
    // mandated convention per slot, not something a per-texture attribute
    // can override in the exported file itself (glTF has no per-texture
    // color-space field). tex.color_space was parsed and stored but never
    // read back anywhere, so an author who explicitly (and incorrectly)
    // marked e.g. a normal map as color_space="srgb" got no signal that
    // their declared intent doesn't match what glTF will actually do with
    // it. Warn (don't fail/silently "fix") when a texture's own declared
    // color_space conflicts with the slot's mandated encoding.
    auto warnIfColorSpaceMismatch = [&](const std::string& texRef, const char* slot,
                                         const char* expectedSpace) {
        auto texIt = textures.find(texRef);
        if (texIt == textures.end()) return;
        const std::string& declared = texIt->second.colorSpace;
        if (!declared.empty() && declared != expectedSpace) {
            std::cerr << "Warning: material '" << mat.name << "' texture '" << texRef
                      << "' used as " << slot << " declares color_space=\"" << declared
                      << "\", but glTF requires " << expectedSpace << " for this slot -- "
                      << "the exported file follows the glTF convention regardless "
                      << "(no per-texture color-space override exists in glTF 2.0)\n";
            ++warningCount;
        }
    };

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
            warnIfColorSpaceMismatch(mat.baseColorTexture, "base_color_texture", "srgb");
        } else {
            warnIfUnresolvedSvg(mat.baseColorTexture, "base_color_texture");
        }
    }
    if (!mat.metallicRoughnessTexture.empty()) {
        auto it = texIdx.find(mat.metallicRoughnessTexture);
        if (it != texIdx.end()) {
            pbr.metallicRoughnessTexture.index    = it->second;
            pbr.metallicRoughnessTexture.texCoord = 0;
            warnIfColorSpaceMismatch(mat.metallicRoughnessTexture, "metallic_roughness_texture", "linear");
        } else {
            warnIfUnresolvedSvg(mat.metallicRoughnessTexture, "metallic_roughness_texture");
        }
    }
    if (!mat.normalTexture.empty()) {
        auto it = texIdx.find(mat.normalTexture);
        if (it != texIdx.end()) {
            m.normalTexture.index    = it->second;
            m.normalTexture.texCoord = 0;
            m.normalTexture.scale    = mat.normalScale;
            warnIfColorSpaceMismatch(mat.normalTexture, "normal_texture", "linear");
        } else {
            warnIfUnresolvedSvg(mat.normalTexture, "normal_texture");
        }
    }
    if (!mat.occlusionTexture.empty()) {
        auto it = texIdx.find(mat.occlusionTexture);
        if (it != texIdx.end()) {
            m.occlusionTexture.index    = it->second;
            m.occlusionTexture.texCoord = 0;
            m.occlusionTexture.strength = mat.occlusionStrength;
            warnIfColorSpaceMismatch(mat.occlusionTexture, "occlusion_texture", "linear");
        } else {
            warnIfUnresolvedSvg(mat.occlusionTexture, "occlusion_texture");
        }
    }
    if (!mat.emissiveTexture.empty()) {
        auto it = texIdx.find(mat.emissiveTexture);
        if (it != texIdx.end()) {
            m.emissiveTexture.index    = it->second;
            m.emissiveTexture.texCoord = 0;
            warnIfColorSpaceMismatch(mat.emissiveTexture, "emissive_texture", "srgb");
        } else {
            warnIfUnresolvedSvg(mat.emissiveTexture, "emissive_texture");
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
        // Untrusted-input containment: a path-traversal / absolute mesh source is
        // a hard error (thrown outside the try below), not a silently-skipped
        // warning like a merely-missing file.
        assertResourceAllowed(ctx.basePath, obj.meshSource, ctx.allowExternalResources,
                              "mesh source");
        try {
            md = loadObjMesh(ctx.basePath, obj.meshSource);
        } catch (const std::exception& e) {
            std::cerr << "Warning: " << e.what() << '\n';
            ctx.stats.warnings++;
            return -1;
        }
    } else if (obj.extrude.has_value()) {
        md = buildExtrude(*obj.extrude);
    } else if (obj.primitive.has_value() && obj.type != ObjectType::Area) {
        // Area's `primitive` (STAB-0031) only stores its `size` attribute for
        // round-tripping/editor use — an area is a non-rendering marker
        // volume, not a mesh, regardless of primitiveType's default value.
        md = buildPrimitive(*obj.primitive);
    }

    if (obj.deform.has_value()) {
        md.applyScale(obj.deform->scale[0],
                      obj.deform->scale[1],
                      obj.deform->scale[2]);
    }

    // AUD-024: per-object UV mapping (scale/offset/rotation) was previously
    // silently ignored by the exporter -- authored uvMapping round-tripped
    // through XML/MCB but never affected the actual exported TEXCOORD_0.
    // Box/Sphere projection genuinely isn't implemented anywhere (editor
    // viewport or exporter both only ever emit the primitive's default
    // planar unwrap), so that part is truthfully warned about rather than
    // silently dropped or falsely claimed as applied.
    if (obj.uvMapping.has_value()) {
        const auto& uv = *obj.uvMapping;
        md.applyUvMapping(uv.scaleU, uv.scaleV, uv.offsetU, uv.offsetV, uv.rotation);
        if (uv.projection != UvProjection::Planar) {
            std::cerr << "[mc3togltf] Warning: object '" << obj.name
                      << "' uses uv_mapping projection '"
                      << (uv.projection == UvProjection::Box ? "box" : "sphere")
                      << "' -- projection-based UV generation is not implemented, only "
                         "scale/offset/rotation were applied to the default unwrap.\n";
            ctx.stats.warnings++;
        }
    }

    // Apply unit scale to geometry positions
    if (ctx.unitScale != 1.0f)
        md.applyScale(ctx.unitScale, ctx.unitScale, ctx.unitScale);

    if (md.empty()) return -1;

    // STAB-0687: accessors are created in this exact order (position, then
    // conditionally normal/texcoord, then indices) to match the pre-existing
    // golden-byte-output test (mc3togltf_golden) -- reordering accessor
    // creation, even without changing any actual content, changes their
    // glTF indices and fails that byte-exact comparison.
    int posAcc = addAccessorVec3(model, md.positions, /*calcBounds=*/true);
    int normAcc = !md.normals.empty()   ? addAccessorVec3(model, md.normals)   : -1;
    int uvAcc   = !md.texcoords.empty() ? addAccessorVec2(model, md.texcoords) : -1;
    int idxAcc = addAccessorIndices(model, md.indices);

    // STAB-0664: normal-mapped meshes need TANGENT for correct tangent-
    // space normal mapping (glTF viewers may fall back to derivative-based
    // tangents without it, but that's inconsistent across implementations
    // and can look visibly wrong, especially near UV seams). Only computed
    // when actually needed (a normal map is assigned) and possible (both
    // NORMAL and TEXCOORD_0 are present) -- added after idxAcc so existing
    // meshes' POSITION/NORMAL/TEXCOORD_0/indices accessor order and indices
    // are completely unaffected (STAB-0687's golden-byte-test constraint).
    int tanAcc = -1;
    if (normAcc >= 0 && uvAcc >= 0 && materialIdx >= 0 &&
        materialIdx < static_cast<int>(model.materials.size()) &&
        model.materials[materialIdx].normalTexture.index >= 0)
    {
        std::vector<float> tangents = computeTangents(md.positions, md.normals, md.texcoords, md.indices);
        if (!tangents.empty())
            tanAcc = addAccessorVec4(model, tangents);
    }

    tinygltf::Primitive prim;
    prim.attributes["POSITION"] = posAcc;
    // Guard against a spec-invalid zero-count accessor if md ever has
    // positions/indices but no normals/texcoords, matching the sibling
    // addMeshDataToGltf()'s existing TEXCOORD_0 guard (which this function
    // previously lacked entirely, for both attributes).
    if (normAcc >= 0) prim.attributes["NORMAL"] = normAcc;
    if (uvAcc   >= 0) prim.attributes["TEXCOORD_0"] = uvAcc;
    if (tanAcc  >= 0) prim.attributes["TANGENT"] = tanAcc;
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
    int uvAcc = -1;
    if (!md.texcoords.empty()) {
        uvAcc = addAccessorVec2(model, md.texcoords);
        prim.attributes["TEXCOORD_0"] = uvAcc;
    }
    // STAB-0664: same TANGENT generation as buildMesh() -- see that
    // function's comment for the algorithm/rationale. CSG-evaluated meshes
    // (this function's only caller) can carry a normal-mapped material too.
    if (uvAcc >= 0 && materialIdx >= 0 &&
        materialIdx < static_cast<int>(model.materials.size()) &&
        model.materials[materialIdx].normalTexture.index >= 0)
    {
        std::vector<float> tangents = computeTangents(md.positions, md.normals, md.texcoords, md.indices);
        if (!tangents.empty()) {
            int tanAcc = addAccessorVec4(model, tangents);
            prim.attributes["TANGENT"] = tanAcc;
        }
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

// A definition whose subtree contains an <instance> of itself expands forever.
// buildNode has no visited-set, so a depth cap is the backstop that turns a
// cyclic/pathologically-deep instance graph into a clear error instead of a
// stack-overflow crash. 256 is far deeper than any legitimate hierarchy.
static constexpr int kMaxNodeDepth = 256;

static int buildNode(ExportCtx& ctx, const Mc3Object& obj, int depth)
{
    ctx.stats.objectsProcessed++;

    if (depth > kMaxNodeDepth)
        throw std::runtime_error(
            "object/instance nesting exceeds " + std::to_string(kMaxNodeDepth) +
            " levels — possible cyclic <instance> definition");

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
        auto q = eulerToQuat(t.rotation[0], t.rotation[1], t.rotation[2],
                              ctx.rotationIsRadians, ctx.eulerOrder);
        node.rotation = {q[0], q[1], q[2], q[3]};
    }

    if (t.scale[0] != 1.0f || t.scale[1] != 1.0f || t.scale[2] != 1.0f)
        node.scale = {t.scale[0], t.scale[1], t.scale[2]};

    // --- Material (materialOverride takes priority) ---
    int matIdx = -1;
    const std::string& matName = !obj.materialOverride.empty() ? obj.materialOverride : obj.material;
    if (!matName.empty()) {
        auto it = ctx.matNameToIdx.find(matName);
        if (it != ctx.matNameToIdx.end()) {
            matIdx = it->second;
        } else {
            // STAB-0679: was silently dropped, inconsistent with the
            // analogous dangling-SVG-texture case (warnIfUnresolvedSvg
            // above), which does warn.
            std::cerr << "Warning: object '" << obj.name << "' references "
                         "unknown material '" << matName << "' — exported without a material.\n";
            ctx.stats.warnings++;
        }
    }

    // --- Geometry ---
    // Nodes that own direct geometry: all types with a primitive or extrude,
    // plus Instance (resolved via definitions).
    int directMesh = -1;

    if (obj.type == ObjectType::Instance && !obj.resolvedInstanceDefinitionKey().empty()) {
        const std::string& defKey = obj.resolvedInstanceDefinitionKey();
        auto it = ctx.definitions.find(defKey);
        if (it != ctx.definitions.end() && it->second) {
            const Mc3Object& defObj = *it->second;

            int effectiveMat = matIdx >= 0 ? matIdx : [&]{
                auto jt = ctx.matNameToIdx.find(defObj.material);
                return jt != ctx.matNameToIdx.end() ? jt->second : -1;
            }();

            // Reuse cached mesh if same definition+material+deform was already built
            auto cacheKey = buildDefCacheKey(defKey, effectiveMat, obj.deform);
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
                int ci = buildNode(ctx, *child, depth + 1);
                if (ci >= 0) node.children.push_back(ci);
            }
        } else {
            std::cerr << "Warning: instance references unknown definition '"
                      << defKey << "'\n";
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
            int ci = buildNode(ctx, *child, depth + 1);
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
        // AUD-029: obj.metadata is an opaque string->string pass-through
        // (mirrors <metadata> in mc3/mcb) that survives every other
        // round-trip but was never read here, unlike tags/collision --
        // silently dropped on glTF export with no warning.
        if (!obj.metadata.empty()) {
            tinygltf::Value::Object metaObj;
            for (const auto& [key, value] : obj.metadata)
                metaObj[key] = tinygltf::Value(value);
            extras["metadata"] = tinygltf::Value(metaObj);
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
                      std::vector<int>& outLightNodeIndices,
                      float unitScale,
                      int& warningCount)
{
    if (lights.empty()) return;

    tinygltf::Value::Array lightsArray;

    for (const auto& light : lights) {
        if (light.type == LightType::Ambient) {
            // STAB-0696: glTF 2.0 / KHR_lights_punctual has no ambient light
            // type -- warn instead of silently dropping it, matching the
            // CSG approximate-mode "can't fully represent this" pattern.
            std::cerr << "[mc3togltf] Warning: ambient light '" << light.name
                      << "' has no glTF equivalent, omitted from export.\n";
            ++warningCount;
            continue;
        }

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

        // Range is a distance, so it scales with the document's unit scale —
        // matching geometry and camera positions (STAB-0693).
        if (light.range > 0.0f)
            lo["range"] = tinygltf::Value(static_cast<double>(light.range) * unitScale);

        if (light.type == LightType::Spot) {
            // STAB-0692: light.angle is already a half-angle in degrees
            // (Mc3Light.hpp), so the radian conversion is /180, not /360 --
            // the old /360 halved every exported spotlight's cone.
            double halfAngle  = light.angle * std::numbers::pi / 180.0;
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

        // Directional lights are purely rotational (glTF ignores their
        // position). Spot lights are BOTH positioned and aimed; point lights are
        // positioned only. Previously spot lights took the rotation-only branch,
        // so every spotlight exported at the world origin regardless of its
        // authored position. Positions scale with unitScale like all geometry
        // and camera nodes (STAB-0693).
        if (light.type == LightType::Directional || light.type == LightType::Spot) {
            auto q = directionToQuat(light.direction[0],
                                     light.direction[1],
                                     light.direction[2]);
            lnode.rotation = {q[0], q[1], q[2], q[3]};
        }
        if (light.type == LightType::Spot || light.type == LightType::Point) {
            lnode.translation = {
                static_cast<double>(light.position[0]) * unitScale,
                static_cast<double>(light.position[1]) * unitScale,
                static_cast<double>(light.position[2]) * unitScale
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

    if (lightsArray.empty()) return;  // all lights were ambient-only (STAB-0696)

    model.extensionsUsed.push_back("KHR_lights_punctual");
    tinygltf::Value::Object extObj;
    extObj["lights"] = tinygltf::Value(lightsArray);
    model.extensions["KHR_lights_punctual"] = tinygltf::Value(extObj);
}

// ---------------------------------------------------------------------------
// Cameras
// ---------------------------------------------------------------------------

static void addCameraNodes(tinygltf::Model& model,
                           const std::vector<Mc3Camera>& cameras,
                           std::vector<int>& outCameraNodeIndices,
                           float unitScale,
                           bool rotationIsRadians,
                           const std::string& eulerOrder)
{
    for (const auto& cam : cameras) {
        tinygltf::Camera gcam;
        gcam.name = cam.name;

        if (cam.type == CameraType::Perspective) {
            gcam.type = "perspective";
            gcam.perspective.yfov  = cam.fov * std::numbers::pi / 180.0;
            gcam.perspective.znear = cam.nearPlane;
            gcam.perspective.zfar  = cam.farPlane;
            // STAB-0694: aspectRatio was hardcoded to 16:9 for every camera
            // regardless of intended output. Mc3Camera has no per-camera
            // aspect field to derive a real value from, and the glTF spec's
            // own recommended behavior when aspectRatio is omitted is for
            // the viewer to use its actual viewport's aspect ratio -- leave
            // it unset (tinygltf only serializes aspectRatio when > 0).
        } else {
            gcam.type = "orthographic";
            // STAB-0695: orthoAspect defaults to 1.0 (square), so this is a
            // no-op for every camera authored before the field existed.
            gcam.orthographic.xmag  = cam.orthoSize * cam.orthoAspect;
            gcam.orthographic.ymag  = cam.orthoSize;
            gcam.orthographic.znear = cam.nearPlane;
            gcam.orthographic.zfar  = cam.farPlane;
        }

        int camIdx = static_cast<int>(model.cameras.size());
        model.cameras.push_back(std::move(gcam));

        tinygltf::Node cnode;
        cnode.name   = cam.name;
        cnode.camera = camIdx;
        // STAB-0693: cameras previously used raw, un-scaled positions while
        // every other node's translation is scaled by unitScale (metres
        // conversion for unit="centimeter"/"inch" documents) -- a camera in
        // a non-meter document ended up at the wrong distance from the
        // correctly-scaled geometry around it.
        cnode.translation = {
            static_cast<double>(cam.position[0]) * unitScale,
            static_cast<double>(cam.position[1]) * unitScale,
            static_cast<double>(cam.position[2]) * unitScale
        };

        if (cam.rotation.has_value()) {
            const auto& r = *cam.rotation;
            auto q = eulerToQuat(r[0], r[1], r[2], rotationIsRadians, eulerOrder);
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

    // STAB-0698: skyboxTexture (equirectangular panorama, "I2") was missing
    // here, unlike its sibling backgroundTexture ("I1") just above.
    if (!env->skyboxTexture.empty())
        extras["skyboxTexture"] = tinygltf::Value(env->skyboxTexture);

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
    float unitScale,
    bool rotationIsRadians,
    const std::string& eulerOrder,
    int& warningCount)
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
                    ++warningCount;
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
            if (nodeIt == nodeNameMap.end()) {
                // STAB-0682: was a silent no-op, unlike the unsupported-
                // property skip a few lines above (which does warn) --
                // a typo'd targetObject silently did nothing with no signal.
                std::cerr << "Warning: mc3togltf: action '" << actionName
                          << "': channel target '" << objName
                          << "' does not match any exported node — channels "
                             "for this target skipped.\n";
                ++warningCount;
                continue;
            }
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

                // Dense sampling for cubic bezier to preserve curve shape in
                // LINEAR glTF output (deliberately NOT glTF's own CUBICSPLINE
                // sampler mode, which would need real in/out tangent data in
                // a strict triple-per-keyframe layout -- baking to dense
                // LINEAR samples via the same evaluateChannel() the live
                // editor uses guarantees editor/export visual parity instead).
                //
                // STAB-0683: 30 samples/sec is a fixed rate, not adaptive to
                // curve complexity or action duration -- chosen as a common
                // "looks smooth" video/animation frame rate, not derived from
                // any curvature/velocity analysis. No cap on total samples:
                // a very long bezier-interpolated action produces a
                // correspondingly large accessor purely from duration, and a
                // very fast/sharp curve in a short window could in principle
                // be under-sampled. Not changed here -- would need adaptive
                // resampling (denser where curvature/velocity is high) to
                // meaningfully improve on a fixed rate, a real but separate
                // feature, not attempted in this pass.
                // AUD-028: a plain (non-cubic) LINEAR rotation channel is
                // exported using only its original keyframe times. glTF's
                // LINEAR interpolation for the "rotation" path is spherical
                // linear interpolation (slerp) between the per-keyframe
                // quaternions, which always takes the shortest arc -- so a
                // euler component that sweeps more than 180 degrees between
                // two consecutive raw keyframes (e.g. 0->270) is reproduced
                // as the -90-degree short path instead of the authored
                // sweep. Detect that case and reuse the same dense-sampling
                // fix already used for cubic bezier below: baking extra
                // LINEAR samples in between via evaluateChannel() (the same
                // function the live editor uses) makes consecutive samples'
                // quaternion delta small enough that slerp tracks the
                // authored euler path instead of re-pathing it. STEP
                // channels are exempt -- they never interpolate, so the
                // shortest-arc issue does not apply.
                bool needsDenseRotation = false;
                if (path == "rotation" && !hasCubic && !allStep) {
                    std::vector<float> rawTimes(timeSet.begin(), timeSet.end());
                    constexpr float kPi = 3.14159265358979323846f;
                    const float fullTurn = rotationIsRadians ? (2.0f * kPi) : 360.0f;
                    for (size_t i = 1; i < rawTimes.size() && !needsDenseRotation; ++i) {
                        for (int c = 0; c < 3; ++c) {
                            if (!pg.ch[c]) continue;
                            float v0 = evaluateChannel(*pg.ch[c], rawTimes[i - 1]);
                            float v1 = evaluateChannel(*pg.ch[c], rawTimes[i]);
                            if (std::fabs(v1 - v0) > fullTurn * 0.5f) {
                                needsDenseRotation = true;
                                break;
                            }
                        }
                    }
                }

                if (hasCubic || needsDenseRotation) {
                    float minT = *timeSet.begin(), maxT = *timeSet.rbegin();
                    constexpr float kBezierBakeSampleRateHz = 30.0f;
                    for (float t = minT; t <= maxT + 1e-5f; t += 1.0f / kBezierBakeSampleRateHz)
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
                        auto q = eulerToQuat(euler[0], euler[1], euler[2], rotationIsRadians, eulerOrder);
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
                            // AUD-027: the static pose's node.translation is
                            // position + pivot (buildNode above), but
                            // evaluateChannel()/base[] here only ever know
                            // about position -- baseTransforms stores the
                            // pivot-free Mc3Transform, and animated
                            // keyframes carry raw position values, never
                            // position+pivot. Left unadjusted, playing any
                            // translation channel overwrote the node's
                            // static translation with a pivot-free value,
                            // snapping the object by -pivot the instant the
                            // animation started. Add the same pivot offset
                            // the static pose uses so animated and static
                            // poses agree.
                            if (path == "translation") v += baseT.pivot[i];
                            valueData.push_back(v * tScale);
                        }
                    }
                }

                // Time accessor (SCALAR, min/max required by glTF spec).
                //
                // STAB-0460: bake the action's playback-speed multiplier
                // into the exported keyframe times, so a standard glTF
                // viewer -- which has no notion of "time scale" -- still
                // reproduces the same real-time playback speed the editor
                // shows (2x speed -> keyframe times halved -> exported
                // animation finishes in half the real-world time). `times`
                // itself must stay unscaled: it's also used above to sample
                // evaluateChannel(), which binary-searches against the
                // keyframes' own unscaled time domain.
                {
                    float invTimeScale = (action.timeScale > 1e-6f) ? 1.0f / action.timeScale : 1.0f;
                    std::vector<float> outTimes;
                    outTimes.reserve(times.size());
                    for (float t : times) outTimes.push_back(t * invTimeScale);

                    int bv = addBufferView(model, outTimes.data(), outTimes.size() * sizeof(float), 0);
                    tinygltf::Accessor acc;
                    acc.bufferView    = bv;
                    acc.byteOffset    = 0;
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    acc.count         = static_cast<int>(outTimes.size());
                    acc.type          = TINYGLTF_TYPE_SCALAR;
                    acc.minValues     = {static_cast<double>(outTimes.front())};
                    acc.maxValues     = {static_cast<double>(outTimes.back())};
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

        if (!anim.channels.empty()) {
            // STAB-0681: glTF core animation spec has no "autoplay on load"
            // or "loop" concept (a runtime/engine decision, not exportable
            // data) -- preserved via extras instead, matching this file's
            // own established convention (node tags/collision, light
            // castShadows, environment, asset mc3_version) for otherwise-
            // inexpressible mc3 data, which this was previously missing.
            tinygltf::Value::Object extras;
            extras["autoplay"]   = tinygltf::Value(action.autoplay);
            extras["loop"]       = tinygltf::Value(action.loop);
            // STAB-0460: the raw multiplier, for round-trip/tooling use --
            // independent of the baked keyframe-time scaling above, which
            // is what makes a plain glTF viewer actually play back at the
            // right speed.
            extras["time_scale"] = tinygltf::Value(static_cast<double>(action.timeScale));
            anim.extras = tinygltf::Value(extras);
            model.animations.push_back(std::move(anim));
        }
    }
}

// ---------------------------------------------------------------------------
// GltfExporter::exportDocument
// ---------------------------------------------------------------------------

void GltfExporter::exportDocument(const Mc3Document& doc,
                                   const std::filesystem::path& outputPath,
                                   OutputFormat format)
{
    // SYS-W1-01: re-validate doc's current in-memory state before building
    // any glTF output -- see the `validation` member's doc comment. Runs
    // first (and unconditionally) so it covers documents that fail later in
    // this function too, not just ones that export successfully.
    validation.clear();
    doc.validate(validation);

    tinygltf::Model model;
    tinygltf::TinyGLTF writer;

    // STAB-0416: tinygltf's default image writer (tinygltf::WriteImageData)
    // truncates image.uri down to just its basename (GetBaseFilename) before
    // writing it out — designed for the "auto-write image bytes next to the
    // gltf" workflow, but it silently drops any subdirectory prefix (e.g.
    // "textures/wall.png" -> "wall.png") for external-reference textures,
    // which never had pixel data for it to write in the first place. Only
    // override the external-reference case (image->image.empty()); delegate
    // to the real default for embedded images so --embed/.glb is unaffected.
    writer.SetImageWriter(
        [](const std::string* basepath, const std::string* filename,
           const tinygltf::Image* image, bool embedImages,
           const tinygltf::FsCallbacks* fs_cb, const tinygltf::URICallbacks* uri_cb,
           std::string* out_uri, void* user_data) -> bool {
            if (image->image.empty()) {
                *out_uri = image->uri;
                return true;
            }
            return tinygltf::WriteImageData(basepath, filename, image, embedImages,
                                             fs_cb, uri_cb, out_uri, user_data);
        },
        nullptr);

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

    // AUD-026: buildTextures/buildMaterial run before ExportCtx exists (its
    // matNameToIdx is built FROM buildMaterial's output), so their warnings
    // can't be counted into ctx.stats.warnings directly -- accumulate into a
    // local first and fold it in right after ctx is constructed below.
    int preCtxWarnings = 0;

    // Textures (embedImages=true for GLB so images are embedded as data URIs)
    bool embedImagesNow = (format == OutputFormat::GLB);
    auto texIdx = buildTextures(model, doc.textures, doc.sourcePath,
                                 outputPath.parent_path(), embedImagesNow,
                                 allowExternalResources, preCtxWarnings);

    // Materials
    std::unordered_map<std::string, int> matNameToIdx;
    for (const auto& [name, mat] : doc.materials) {
        int idx = buildMaterial(model, mat, texIdx, doc.svgTextures, doc.textures, preCtxWarnings);
        matNameToIdx[name] = idx;
    }

    // Scene
    tinygltf::Scene scene;
    scene.name = doc.model.empty() ? "Scene" : doc.model;

    // Object nodes (recursive)
    ExportCtx ctx{model, matNameToIdx, doc.definitions,
                  unitScaleFactor(doc.unit), doc.sourcePath,
                  allowApproximateCSG, allowExternalResources,
                  doc.rotationUnits == "radians", doc.eulerOrder,
                  {}, {}, {}};
    ctx.stats.warnings += preCtxWarnings;
    for (const auto& objPtr : doc.objects) {
        if (!objPtr) continue;
        int nodeIdx = buildNode(ctx, *objPtr);
        if (nodeIdx >= 0) scene.nodes.push_back(nodeIdx);
    }

    // Lights
    std::vector<int> lightNodes;
    addLights(model, doc.lights, lightNodes, ctx.unitScale, ctx.stats.warnings);
    for (int i : lightNodes) scene.nodes.push_back(i);

    // Cameras
    std::vector<int> cameraNodes;
    addCameraNodes(model, doc.cameras, cameraNodes,
                    ctx.unitScale, ctx.rotationIsRadians, ctx.eulerOrder);
    for (int i : cameraNodes) scene.nodes.push_back(i);

    // Animations
    if (!doc.actions.empty()) {
        std::unordered_map<std::string, Mc3Transform> baseTransforms;
        collectBaseTransforms(doc.objects, baseTransforms);

        // STAB-0686: mc3 only enforces id uniqueness, not name uniqueness --
        // Mc3Channel::targetObject targets by NAME, so two same-named nodes
        // silently collide here (last-write-wins) and only the later one
        // receives its animation; the earlier one is silently unanimated.
        std::unordered_map<std::string, int> nodeNameMap;
        for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
            const std::string& name = model.nodes[i].name;
            if (name.empty()) continue;
            if (nodeNameMap.count(name)) {
                std::cerr << "[mc3togltf] Warning: duplicate node name '" << name
                          << "' -- animation channels targeting this name will "
                             "only affect the last node with that name.\n";
                ctx.stats.warnings++;
            }
            nodeNameMap[name] = i;
        }

        exportAnimations(model, doc.actions, nodeNameMap, baseTransforms, ctx.unitScale,
                          ctx.rotationIsRadians, ctx.eulerOrder, ctx.stats.warnings);
    }

    // Environment → scene extras
    applyEnvironment(scene, doc.environment);

    model.scenes.push_back(std::move(scene));
    model.defaultScene = 0;

    bool writeBinary = (format == OutputFormat::GLB);
    bool embedImages = writeBinary;
    bool prettyPrint = !writeBinary;

    // AUDIT-0019: for GLB, the output is always a single self-contained
    // file, so we can safely write to a sibling temp path and rename over
    // the real destination only after a fully successful write -- a crash/
    // disk-full/permission failure mid-write can then never leave a
    // truncated .glb at the path the user asked to export to.
    //
    // For plain .gltf, tinygltf also writes a separate external .bin buffer
    // (and possibly image files) alongside the JSON, with the buffer's
    // on-disk name and the JSON's internal "uri" reference to it both
    // derived from the given path's filename. Writing the JSON to a
    // differently-named temp path would make tinygltf emit a buffer
    // reference that no longer matches the real destination's expected
    // sibling filename once renamed -- a subtler, worse corruption than the
    // truncation this fix prevents. Left non-atomic for that multi-file
    // case pending a proper multi-file-aware fix.
    if (writeBinary) {
        std::filesystem::path tmpPath = outputPath;
        tmpPath += ".tmp";
        std::string tmpPathStr = tmpPath.string();
        bool ok = writer.WriteGltfSceneToFile(&model, tmpPathStr,
                                               embedImages,
                                               /*embedBuffers=*/writeBinary,
                                               prettyPrint,
                                               writeBinary);
        if (!ok) {
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);
            throw std::runtime_error("tinygltf: failed to write " + tmpPathStr);
        }
        std::error_code ec;
        std::filesystem::rename(tmpPath, outputPath, ec);
        if (ec) {
            std::filesystem::remove(tmpPath, ec);
            throw std::runtime_error("Failed to finalize GLB export (rename): " + outputPath.string());
        }
    } else {
        std::string path = outputPath.string();
        bool ok = writer.WriteGltfSceneToFile(&model, path,
                                               embedImages,
                                               /*embedBuffers=*/writeBinary,
                                               prettyPrint,
                                               writeBinary);
        if (!ok)
            throw std::runtime_error("tinygltf: failed to write " + path);
    }

    // Populate export statistics from accumulated ctx.stats + model aggregate counts.
    stats = ctx.stats;
    stats.gltfNodes     = static_cast<int>(model.nodes.size());
    stats.uniqueMeshes  = static_cast<int>(model.meshes.size());
    stats.materialCount = static_cast<int>(model.materials.size());
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
