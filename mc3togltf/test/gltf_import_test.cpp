// SYS-W14-36: bounded editable GLB import must keep the source as a native
// MC3 hierarchy with per-primitive selectors, PBR/image data, cameras and
// KHR_lights_punctual -- and that MC3 must export back to valid glTF.

#include "GltfExporter.hpp"
#include "GltfImporter.hpp"
#include "MeshBuilder.hpp"

#include <tiny_gltf.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace MeshCraft::Mc3;
using namespace mc3togltf;

static int failures = 0;

static void check(bool condition, const std::string& message) {
    if (condition) std::cout << "PASS: " << message << '\n';
    else { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

static void appendBytes(std::vector<unsigned char>& output, const void* data, size_t bytes) {
    const auto* begin = static_cast<const unsigned char*>(data);
    output.insert(output.end(), begin, begin + bytes);
}

static tinygltf::Model makeTriangleModel(bool triangleMode = true) {
    tinygltf::Model model;
    model.asset.version = "2.0";
    model.asset.generator = "mc3togltf_gltf_import_test";

    const std::array<float, 9> positions{-1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    const std::array<uint16_t, 3> indices{0, 1, 2};
    tinygltf::Buffer buffer;
    appendBytes(buffer.data, positions.data(), sizeof(positions));
    appendBytes(buffer.data, indices.data(), sizeof(indices));
    model.buffers.push_back(std::move(buffer));

    tinygltf::BufferView positionView;
    positionView.buffer = 0;
    positionView.byteOffset = 0;
    positionView.byteLength = static_cast<int>(sizeof(positions));
    positionView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    model.bufferViews.push_back(positionView);
    tinygltf::BufferView indexView;
    indexView.buffer = 0;
    indexView.byteOffset = static_cast<int>(sizeof(positions));
    indexView.byteLength = static_cast<int>(sizeof(indices));
    indexView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    model.bufferViews.push_back(indexView);

    tinygltf::Accessor position;
    position.bufferView = 0;
    position.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    position.count = 3;
    position.type = TINYGLTF_TYPE_VEC3;
    position.minValues = {-1.0, 0.0, 0.0};
    position.maxValues = {1.0, 1.0, 0.0};
    model.accessors.push_back(position);
    tinygltf::Accessor index;
    index.bufferView = 1;
    index.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    index.count = 3;
    index.type = TINYGLTF_TYPE_SCALAR;
    model.accessors.push_back(index);

    // Valid 1x1 transparent PNG; preserving it proves the importer maps
    // embedded image bytes to an MC3 data URI and the exporter reinserts them.
    static constexpr std::array<unsigned char, 68> png{
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x04,0x00,0x00,0x00,0xb5,0x1c,0x0c,
        0x02,0x00,0x00,0x00,0x0b,0x49,0x44,0x41,0x54,0x78,0xda,0x63,0xfc,0xff,0x1f,0x00,
        0x02,0xeb,0x01,0xf5,0x8f,0x49,0xe1,0x74,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,
        0x42,0x60,0x82};
    tinygltf::Image image;
    image.name = "inline-image";
    image.image.assign(png.begin(), png.end());
    image.as_is = true;
    image.mimeType = "image/png";
    model.images.push_back(std::move(image));
    model.samplers.push_back(tinygltf::Sampler{});
    tinygltf::Texture texture;
    texture.source = 0;
    texture.sampler = 0;
    model.textures.push_back(texture);

    tinygltf::Material material;
    material.name = "paint";
    material.pbrMetallicRoughness.baseColorFactor = {0.2, 0.4, 0.6, 0.8};
    material.pbrMetallicRoughness.metallicFactor = 0.3;
    material.pbrMetallicRoughness.roughnessFactor = 0.7;
    material.pbrMetallicRoughness.baseColorTexture.index = 0;
    material.alphaMode = "BLEND";
    material.doubleSided = true;
    model.materials.push_back(std::move(material));

    tinygltf::Primitive primitive;
    primitive.attributes["POSITION"] = 0;
    primitive.indices = 1;
    primitive.material = 0;
    primitive.mode = triangleMode ? TINYGLTF_MODE_TRIANGLES : TINYGLTF_MODE_LINE;
    tinygltf::Mesh mesh;
    mesh.name = "triangle";
    mesh.primitives.push_back(std::move(primitive));
    model.meshes.push_back(std::move(mesh));

    tinygltf::Node meshNode;
    meshNode.name = "Triangle node";
    meshNode.mesh = 0;
    meshNode.translation = {2.0, 3.0, 4.0};
    model.nodes.push_back(std::move(meshNode));
    tinygltf::Camera camera;
    camera.name = "Imported camera";
    camera.type = "perspective";
    camera.perspective.yfov = 0.9;
    camera.perspective.znear = 0.1;
    camera.perspective.zfar = 100.0;
    model.cameras.push_back(std::move(camera));
    tinygltf::Node cameraNode;
    cameraNode.name = "Camera node";
    cameraNode.camera = 0;
    cameraNode.translation = {0.0, 1.0, 5.0};
    model.nodes.push_back(std::move(cameraNode));

    tinygltf::Light light;
    light.name = "Imported light";
    light.type = "spot";
    light.intensity = 3.0;
    light.color = {0.25, 0.5, 0.75};
    light.spot.innerConeAngle = 0.1;
    light.spot.outerConeAngle = 0.4;
    model.lights.push_back(std::move(light));
    tinygltf::Node lightNode;
    lightNode.name = "Light node";
    lightNode.translation = {1.0, 2.0, 3.0};
    lightNode.light = 0;
    model.nodes.push_back(std::move(lightNode));

    tinygltf::Scene scene;
    scene.nodes = {0, 1, 2};
    model.scenes.push_back(std::move(scene));
    model.defaultScene = 0;
    return model;
}

static bool writeModel(const tinygltf::Model& source, const fs::path& destination, bool binary) {
    tinygltf::Model model = source;
    tinygltf::TinyGLTF writer;
    return writer.WriteGltfSceneToFile(&model, destination.string(), true,
                                       binary ? true : false, false, binary);
}

static const Mc3Object* findImportedMesh(const std::shared_ptr<Mc3Object>& object) {
    if (!object) return nullptr;
    if (object->type == ObjectType::Mesh) return object.get();
    for (const auto& child : object->children)
        if (const auto* mesh = findImportedMesh(child)) return mesh;
    return nullptr;
}

int main() {
    const fs::path root = fs::temp_directory_path() / "mc3togltf_gltf_import_test";
    const fs::path glb = root / "source.glb";
    const fs::path gltf = root / "trusted.gltf";
    const fs::path escapedGltf = root / "nested" / "escape.gltf";
    const fs::path roundTrip = root / "roundtrip.glb";
    const fs::path lineGlb = root / "lines.glb";
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    fs::create_directories(root, cleanupError);

    const tinygltf::Model triangle = makeTriangleModel();
    check(writeModel(triangle, glb, true), "fixture GLB writes");
    {
        tinygltf::TinyGLTF reader;
        tinygltf::Model source;
        std::string error, warning;
        const bool loaded = reader.LoadBinaryFromFile(&source, &error, &warning, glb.string());
        check(loaded && source.lights.size() == 1 && source.nodes.size() > 2 && source.nodes[2].light == 0,
              "fixture preserves KHR_lights_punctual root and node extensions");
    }
    try {
        const auto imported = importSelfContainedGlb(glb);
        check(imported.triangleCount == 1, "triangle count is retained under the import cap");
        check(imported.document.embeds.size() == 1, "source GLB is stored as one inline MC3 embed");
        check(imported.document.objects.size() == 3, "active GLB nodes become editable MC3 root objects");
        check(imported.document.materials.size() == 1 && imported.document.textures.size() == 1,
              "PBR material and embedded image become MC3 registry records");
        if (!imported.document.textures.empty())
            check(imported.document.textures.begin()->second.uri.rfind("data:image/png;base64,", 0) == 0,
                  "embedded image bytes are retained as an MC3 data URI");
        const Mc3Object* mesh = findImportedMesh(imported.document.objects.front());
        check(mesh && mesh->meshSource.rfind("embed:", 0) == 0 &&
              parseEmbeddedGltfSelection(mesh->metadata).has_value(),
              "mesh primitive uses an explicit editable embed selector");
        check(imported.document.cameras.size() == 1 && imported.document.lights.size() == 1,
              "camera and KHR_lights_punctual light are imported from active nodes (got " +
              std::to_string(imported.document.cameras.size()) + " camera(s), " +
              std::to_string(imported.document.lights.size()) + " light(s))");

        GltfExporter exporter;
        exporter.exportDocument(imported.document, roundTrip, OutputFormat::GLB);
        tinygltf::TinyGLTF reader;
        tinygltf::Model output;
        std::string error, warning;
        const bool loaded = reader.LoadBinaryFromFile(&output, &error, &warning, roundTrip.string());
        check(loaded, "imported MC3 re-exports as a readable GLB");
        if (loaded) {
            check(!output.meshes.empty() && !output.materials.empty() && !output.images.empty(),
                  "round-trip GLB retains selected mesh, material and image");
            check(output.extensions.count("KHR_lights_punctual") == 1 && !output.cameras.empty(),
                  "round-trip GLB retains punctual light and camera (got " +
                  std::to_string(output.cameras.size()) + " camera(s), " +
                  std::to_string(output.extensions.count("KHR_lights_punctual")) + " light extension(s))");
        }
    } catch (const std::exception& error) {
        check(false, std::string("self-contained GLB import/round trip: ") + error.what());
    }

    check(writeModel(triangle, gltf, false), "external-resource glTF fixture writes");
    bool untrustedRejected = false;
    try { (void)importSelfContainedGlb(gltf); }
    catch (const std::runtime_error& error) {
        untrustedRejected = std::string(error.what()).find("self-contained .glb") != std::string::npos;
    }
    check(untrustedRejected, "normal route rejects textual .gltf without trust opt-in");
    try {
        const auto trusted = importTrustedGltf(gltf);
        check(trusted.document.embeds.size() == 1 && trusted.triangleCount == 1,
              "trusted .gltf is converted to the same inline bounded representation");
    } catch (const std::exception& error) {
        check(false, std::string("trusted external .gltf import: ") + error.what());
    }

    fs::create_directories(escapedGltf.parent_path(), cleanupError);
    check(writeModel(triangle, escapedGltf, false), "escaped-resource glTF fixture writes");
    const fs::path localBuffer = escapedGltf.parent_path() / "escape.bin";
    const fs::path outsideBuffer = root / "outside.bin";
    fs::rename(localBuffer, outsideBuffer, cleanupError);
    std::ifstream escapeInput(escapedGltf, std::ios::binary);
    std::string escapeJson((std::istreambuf_iterator<char>(escapeInput)), {});
    const size_t bufferName = escapeJson.find("escape.bin");
    if (bufferName != std::string::npos)
        escapeJson.replace(bufferName, std::string("escape.bin").size(), "../outside.bin");
    std::ofstream escapeOutput(escapedGltf, std::ios::binary | std::ios::trunc);
    escapeOutput << escapeJson;
    escapeOutput.close();
    bool escapedRejected = false;
    std::string escapedError;
    try { (void)importTrustedGltf(escapedGltf); }
    catch (const std::runtime_error& error) {
        escapedError = error.what();
        escapedRejected = escapedError.find("escapes the source directory") != std::string::npos;
    }
    check(escapedRejected, "trusted route confines companion resources to the selected glTF directory" +
          (escapedError.empty() ? std::string() : " (got: " + escapedError + ")"));

    check(writeModel(makeTriangleModel(false), lineGlb, true), "non-triangle GLB fixture writes");
    bool lineRejected = false;
    try { (void)importSelfContainedGlb(lineGlb); }
    catch (const std::runtime_error& error) {
        lineRejected = std::string(error.what()).find("non-triangle mesh primitive") != std::string::npos;
    }
    check(lineRejected, "unsupported primitive mode is rejected before import mutates an MC3 document");

    fs::remove_all(root, cleanupError);
    if (failures == 0) {
        std::cout << "All glTF import checks passed.\n";
        return 0;
    }
    std::cerr << failures << " glTF import check(s) FAILED.\n";
    return 1;
}
