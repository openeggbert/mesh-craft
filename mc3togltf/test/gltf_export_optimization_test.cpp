// SYS-W14-37: real, opt-in mesh-attribute quantization must be structural,
// deterministic, report per-object fallbacks, and keep GLB/glTF texture
// behavior explicit rather than exposing disabled UI placeholders.

#include "GltfExporter.hpp"

#include <tiny_gltf.h>

#include <algorithm>
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

static std::vector<unsigned char> readBytes(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}

int main() {
    const fs::path root = fs::temp_directory_path() / "mc3togltf_export_optimization_test";
    const fs::path glb = root / "quantized.glb";
    const fs::path glbRepeat = root / "quantized-repeat.glb";
    const fs::path gltf = root / "quantized.gltf";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root, error);

    // Valid 1x1 PNG used for a normal source-relative image as well as the
    // inline data URI below. It lets the textual-glTF assertion cover the
    // rebase/external path instead of only the inline-data special case.
    const std::vector<unsigned char> onePixelPng = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x04, 0x00, 0x00, 0x00, 0xb5, 0x1c, 0x0c, 0x02, 0x00, 0x00, 0x00,
        0x0b,
        0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xfc, 0xff, 0x1f, 0x00, 0x02,
        0xeb, 0x01, 0xf5, 0x8f, 0x49, 0xe1, 0x74, 0x00, 0x00, 0x00, 0x00, 0x49,
        0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
    };
    {
        std::ofstream output(root / "external.png", std::ios::binary);
        output.write(reinterpret_cast<const char*>(onePixelPng.data()),
                     static_cast<std::streamsize>(onePixelPng.size()));
    }

    Mc3Document doc;
    doc.model = "quantization-test";
    doc.sourcePath = root;
    // Valid 1x1 PNG. The same inline texture must be embedded in GLB while a
    // textual glTF keeps the data URI instead of pretending a file was made.
    doc.textures["inline"] = Mc3Texture("inline", "data:image/png;base64,"
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9Y9J4XQAAAAASUVORK5CYII=");
    doc.textures["external"] = Mc3Texture("external", "external.png");
    Mc3Material material;
    material.name = "paint";
    material.baseColorTexture = "inline";
    doc.materials["paint"] = material;

    auto sphere = Mc3Object::makeSphere("Quantized sphere", 1.0f, 16, "paint");
    sphere->id = "sphere-id";
    doc.addObject(sphere);
    auto tiled = Mc3Object::makePlane("Tiled UV", 2.0f, 2.0f, "paint");
    tiled->id = "tiled-uv-id";
    Mc3UvMapping tiledMapping;
    tiledMapping.scaleU = 2.0f; // valid MC3, intentionally unsuitable for normalized U16 UVs
    tiled->uvMapping = tiledMapping;
    doc.addObject(tiled);

    GltfExporter exporter;
    exporter.quantizeMeshAttributes = true;
    try {
        const ExportEstimate estimate = exporter.estimateDocument(doc, glb, OutputFormat::GLB);
        check(estimate.embedsImages && estimate.embeddedImageBytes > 0 &&
              estimate.estimatedTotalBytes > estimate.estimatedBinaryBytes,
              "GLB preflight estimates geometry, JSON, and embedded image payload");
        const bool preflightTiled = std::any_of(exporter.report.begin(), exporter.report.end(),
            [](const ExportReportEntry& entry) { return entry.objectId == "tiled-uv-id"; });
        check(preflightTiled, "preflight report identifies the object whose tiled UVs remain float32");

        exporter.exportDocument(doc, glb, OutputFormat::GLB);
        check(exporter.stats.quantizedAttributeAccessors > 0 && exporter.stats.narrowedIndexAccessors > 0,
              "opt-in export quantizes supported attributes and narrows compatible indices");
        const bool exportTiled = std::any_of(exporter.report.begin(), exporter.report.end(),
            [](const ExportReportEntry& entry) { return entry.objectId == "tiled-uv-id"; });
        check(exportTiled, "post-export compatibility report retains the MC3 object id");

        tinygltf::TinyGLTF reader;
        tinygltf::Model model;
        std::string loadError, loadWarning;
        const bool loaded = reader.LoadBinaryFromFile(&model, &loadError, &loadWarning, glb.string());
        check(loaded, "quantized GLB is readable by tinygltf");
        if (loaded) {
            check(std::find(model.extensionsUsed.begin(), model.extensionsUsed.end(),
                            "KHR_mesh_quantization") != model.extensionsUsed.end(),
                  "GLB declares KHR_mesh_quantization when quantized attributes are present");
            bool shortNormal = false, shortUv = false, shortIndex = false;
            for (const auto& mesh : model.meshes) for (const auto& primitive : mesh.primitives) {
                if (const auto normal = primitive.attributes.find("NORMAL"); normal != primitive.attributes.end()) {
                    const auto& accessor = model.accessors[normal->second];
                    shortNormal |= accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT && accessor.normalized;
                }
                if (const auto uv = primitive.attributes.find("TEXCOORD_0"); uv != primitive.attributes.end()) {
                    const auto& accessor = model.accessors[uv->second];
                    shortUv |= accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT && accessor.normalized;
                }
                if (primitive.indices >= 0)
                    shortIndex |= model.accessors[primitive.indices].componentType ==
                                  TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
            }
            check(shortNormal && shortUv && shortIndex,
                  "GLB uses signed-normalized normals, in-range unsigned-normalized UVs, and UINT16 indices");
            check(!model.images.empty() && !model.images.front().image.empty(),
                  "GLB embeds inline texture bytes rather than emitting a companion image path");
        }

        GltfExporter repeat;
        repeat.quantizeMeshAttributes = true;
        repeat.exportDocument(doc, glbRepeat, OutputFormat::GLB);
        check(readBytes(glb) == readBytes(glbRepeat), "quantized GLB output is deterministic");

        GltfExporter textual;
        textual.quantizeMeshAttributes = true;
        const ExportEstimate gltfEstimate = textual.estimateDocument(doc, gltf, OutputFormat::GLTF);
        check(!gltfEstimate.embedsImages && gltfEstimate.embeddedImageBytes == 0,
              "textual glTF estimate excludes ordinary external image payloads");
        textual.exportDocument(doc, gltf, OutputFormat::GLTF);
        const std::vector<unsigned char> gltfBytes = readBytes(gltf);
        const std::string gltfJson(gltfBytes.begin(), gltfBytes.end());
        check(gltfJson.find("data:image/png;base64,") != std::string::npos,
              "textual glTF retains an MC3 inline image as a data URI");
        check(gltfJson.find("external.png") != std::string::npos,
              "textual glTF keeps a normal source-relative image as an external URI");
    } catch (const std::exception& exception) {
        check(false, std::string("quantized export workflow: ") + exception.what());
    }

    fs::remove_all(root, error);
    if (failures == 0) {
        std::cout << "All export optimization checks passed.\n";
        return 0;
    }
    std::cerr << failures << " export optimization check(s) FAILED.\n";
    return 1;
}
