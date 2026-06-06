#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include "GltfExporter.hpp"
#include "MathUtils.hpp"
#include "MeshBuilder.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Light.hpp>
#include <MeshCraft/Mc3/Mc3Camera.hpp>
#include <MeshCraft/Mc3/Mc3Environment.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include <algorithm>
#include <cstring>
#include <numbers>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace MeshCraft::Mc3;
namespace mc3togltf {

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
              const std::map<std::string, Mc3Texture>& textures)
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
        img.uri  = tex.uri;

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
    if (!mat.normalTexture.empty()) {
        auto it = texIdx.find(mat.normalTexture);
        if (it != texIdx.end()) {
            m.normalTexture.index    = it->second;
            m.normalTexture.texCoord = 0;
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
    std::transform(am.begin(), am.end(), am.begin(), ::toupper);
    if (am == "BLEND") m.alphaMode = "BLEND";
    else if (am == "MASK") m.alphaMode = "MASK";
    else m.alphaMode = "OPAQUE";

    model.materials.push_back(std::move(m));
    return static_cast<int>(model.materials.size()) - 1;
}

// ---------------------------------------------------------------------------
// Mesh from Mc3Object
// ---------------------------------------------------------------------------

static int buildMesh(tinygltf::Model& model,
                     const Mc3Object& obj,
                     int materialIdx)
{
    MeshData md;

    if (obj.extrude.has_value()) {
        md = buildExtrude(*obj.extrude);
    } else if (obj.primitive.has_value()) {
        md = buildPrimitive(*obj.primitive);
    }

    if (obj.deform.has_value()) {
        md.applyScale(obj.deform->scale[0],
                      obj.deform->scale[1],
                      obj.deform->scale[2]);
    }

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
// Node building (recursive)
// ---------------------------------------------------------------------------

static int buildNode(tinygltf::Model& model,
                     const Mc3Object& obj,
                     const std::unordered_map<std::string, int>& matNameToIdx)
{
    tinygltf::Node node;
    node.name = obj.name;

    const auto& t = obj.transform;

    if (t.position[0] != 0.0f || t.position[1] != 0.0f || t.position[2] != 0.0f) {
        node.translation = {t.position[0], t.position[1], t.position[2]};
    }

    if (t.rotation[0] != 0.0f || t.rotation[1] != 0.0f || t.rotation[2] != 0.0f) {
        auto q = eulerXYZToQuat(t.rotation[0], t.rotation[1], t.rotation[2]);
        node.rotation = {q[0], q[1], q[2], q[3]};
    }

    if (t.scale[0] != 1.0f || t.scale[1] != 1.0f || t.scale[2] != 1.0f) {
        node.scale = {t.scale[0], t.scale[1], t.scale[2]};
    }

    // Build mesh if this object has geometry
    int matIdx = -1;
    if (!obj.material.empty()) {
        auto it = matNameToIdx.find(obj.material);
        if (it != matNameToIdx.end()) matIdx = it->second;
    }

    if (obj.primitive.has_value() || obj.extrude.has_value()) {
        int meshIdx = buildMesh(model, obj, matIdx);
        if (meshIdx >= 0) node.mesh = meshIdx;
    }

    // Recurse into children
    for (const auto& child : obj.children) {
        if (!child) continue;
        int childIdx = buildNode(model, *child, matNameToIdx);
        node.children.push_back(childIdx);
    }

    model.nodes.push_back(std::move(node));
    return static_cast<int>(model.nodes.size()) - 1;
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
        if (light.type == LightType::Ambient) {
            // glTF 2.0 has no ambient light — skip
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

        if (light.range > 0.0f) {
            lo["range"] = tinygltf::Value(static_cast<double>(light.range));
        }

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

        // Node for this light
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
            gcam.perspective.yfov         = cam.fov * std::numbers::pi / 180.0;
            gcam.perspective.znear        = cam.nearPlane;
            gcam.perspective.zfar         = cam.farPlane;
            gcam.perspective.aspectRatio  = 16.0 / 9.0;
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

    // Textures
    auto texIdx = buildTextures(model, doc.textures);

    // Materials
    std::unordered_map<std::string, int> matNameToIdx;
    for (const auto& [name, mat] : doc.materials) {
        int idx = buildMaterial(model, mat, texIdx);
        matNameToIdx[name] = idx;
    }

    // Scene
    tinygltf::Scene scene;
    scene.name = doc.model.empty() ? "Scene" : doc.model;

    // Object nodes
    for (const auto& objPtr : doc.objects) {
        if (!objPtr) continue;
        int nodeIdx = buildNode(model, *objPtr, matNameToIdx);
        scene.nodes.push_back(nodeIdx);
    }

    // Lights
    std::vector<int> lightNodeIndices;
    addLights(model, doc.lights, lightNodeIndices);
    for (int idx : lightNodeIndices) scene.nodes.push_back(idx);

    // Cameras
    std::vector<int> cameraNodeIndices;
    addCameraNodes(model, doc.cameras, cameraNodeIndices);
    for (int idx : cameraNodeIndices) scene.nodes.push_back(idx);

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
    if (!ok) {
        throw std::runtime_error("tinygltf: failed to write " + path);
    }
}

} // namespace mc3togltf
