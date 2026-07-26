#include "GltfImporter.hpp"

#include "MeshBuilder.hpp"

#include <tiny_gltf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mc3togltf {
namespace {

using MeshCraft::Mc3::CameraType;
using MeshCraft::Mc3::LightType;
using MeshCraft::Mc3::Mc3Camera;
using MeshCraft::Mc3::Mc3Light;
using MeshCraft::Mc3::Mc3Material;
using MeshCraft::Mc3::Mc3Object;
using MeshCraft::Mc3::Mc3Texture;
using MeshCraft::Mc3::ObjectType;

// MC3's XML parser caps an inline embed's BASE64 text at 64 MiB. Keep the
// decoded GLB below 3/4 of that cap so a successful GUI import can always be
// saved and reloaded; the lower limit is deliberate, not a UI-only hint.
constexpr uintmax_t kMaxImportGlbBytes = 48ull * 1024ull * 1024ull;
constexpr uintmax_t kMaxTrustedGltfJsonBytes = 16ull * 1024ull * 1024ull;
constexpr uintmax_t kMaxTrustedGltfReadBytes = kMaxTrustedGltfJsonBytes + kMaxImportGlbBytes;
constexpr int kMaxImportNodes = 4096;
constexpr int kMaxImportDepth = 64;

[[noreturn]] void importError(const std::filesystem::path& path, const std::string& detail) {
    throw std::runtime_error("GLB import failed (" + path.string() + "): " + detail);
}

std::string lowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension;
}

std::string safeToken(std::string value, const std::string& fallback) {
    for (char& ch : value) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (!std::isalnum(byte) && ch != '_' && ch != '-') ch = '_';
    }
    while (!value.empty() && value.back() == '_') value.pop_back();
    return value.empty() ? fallback : value;
}

std::string base64Encode(const std::vector<unsigned char>& bytes) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve((bytes.size() + 2) / 3 * 4);
    for (size_t offset = 0; offset < bytes.size(); offset += 3) {
        const uint32_t a = bytes[offset];
        const uint32_t b = offset + 1 < bytes.size() ? bytes[offset + 1] : 0;
        const uint32_t c = offset + 2 < bytes.size() ? bytes[offset + 2] : 0;
        encoded.push_back(alphabet[(a >> 2) & 0x3f]);
        encoded.push_back(alphabet[((a & 0x03) << 4) | (b >> 4)]);
        encoded.push_back(offset + 1 < bytes.size() ? alphabet[((b & 0x0f) << 2) | (c >> 6)] : '=');
        encoded.push_back(offset + 2 < bytes.size() ? alphabet[c & 0x3f] : '=');
    }
    return encoded;
}

bool preserveImageBytes(tinygltf::Image* image, int, std::string*, std::string*, int, int,
                        const unsigned char* bytes, int size, void*) {
    if (!image || !bytes || size < 0) return false;
    image->image.assign(bytes, bytes + size);
    image->as_is = true;
    return true;
}

tinygltf::FsCallbacks denyExternalFileAccess() {
    tinygltf::FsCallbacks callbacks;
    callbacks.FileExists = [](const std::string&, void*) { return false; };
    callbacks.ExpandFilePath = [](const std::string& path, void*) { return path; };
    callbacks.ReadWholeFile = [](std::vector<unsigned char>*, std::string* error,
                                 const std::string&, void*) {
        if (error) *error = "external GLB resource access is disabled";
        return false;
    };
    callbacks.WriteWholeFile = [](std::string* error, const std::string&,
                                  const std::vector<unsigned char>&, void*) {
        if (error) *error = "write is disabled during GLB import";
        return false;
    };
    callbacks.GetFileSizeInBytes = [](size_t*, std::string* error, const std::string&, void*) {
        if (error) *error = "external GLB resource access is disabled";
        return false;
    };
    callbacks.user_data = nullptr;
    return callbacks;
}

struct TrustedGltfReadContext {
    std::filesystem::path root;
    uintmax_t bytesRead{0};
    std::string denial;
};

std::optional<std::filesystem::path> trustedResourcePath(const std::string& raw,
                                                          const TrustedGltfReadContext& context) {
    std::filesystem::path candidate(raw);
    if (candidate.is_relative()) candidate = context.root / candidate;
    std::error_code errorCode;
    candidate = std::filesystem::weakly_canonical(candidate, errorCode);
    if (errorCode) return std::nullopt;
    const std::filesystem::path relative = std::filesystem::relative(candidate, context.root, errorCode);
    if (errorCode || relative.empty() || *relative.begin() == "..") return std::nullopt;
    return candidate;
}

bool trustedFileExists(const std::string& raw, void* userData) {
    auto* context = static_cast<TrustedGltfReadContext*>(userData);
    const auto path = context ? trustedResourcePath(raw, *context) : std::nullopt;
    if (context && !path) context->denial = "trusted glTF resource escapes the source directory";
    std::error_code errorCode;
    return path && std::filesystem::is_regular_file(*path, errorCode) && !errorCode;
}

bool trustedReadWholeFile(std::vector<unsigned char>* output, std::string* error,
                          const std::string& raw, void* userData) {
    auto* context = static_cast<TrustedGltfReadContext*>(userData);
    const auto path = context ? trustedResourcePath(raw, *context) : std::nullopt;
    if (!path) {
        if (context) context->denial = "trusted glTF resource escapes the source directory";
        if (error) *error = "trusted glTF resource escapes the source directory";
        return false;
    }
    std::error_code errorCode;
    const uintmax_t bytes = std::filesystem::file_size(*path, errorCode);
    if (errorCode || bytes > kMaxImportGlbBytes ||
        context->bytesRead > kMaxTrustedGltfReadBytes - bytes) {
        context->denial = "trusted glTF resource exceeds the bounded import budget";
        if (error) *error = "trusted glTF resource exceeds the bounded import budget";
        return false;
    }
    std::ifstream stream(*path, std::ios::binary);
    if (!stream) {
        if (error) *error = "trusted glTF resource cannot be opened";
        return false;
    }
    output->assign(std::istreambuf_iterator<char>(stream), {});
    if (output->size() != bytes) {
        if (error) *error = "trusted glTF resource could not be read completely";
        return false;
    }
    context->bytesRead += bytes;
    return true;
}

bool trustedGetFileSize(size_t* bytes, std::string* error, const std::string& raw, void* userData) {
    auto* context = static_cast<TrustedGltfReadContext*>(userData);
    const auto path = context ? trustedResourcePath(raw, *context) : std::nullopt;
    std::error_code errorCode;
    const uintmax_t size = path ? std::filesystem::file_size(*path, errorCode) : 0;
    if (!path || errorCode || size > kMaxImportGlbBytes || size > std::numeric_limits<size_t>::max()) {
        if (context) context->denial = !path ? "trusted glTF resource escapes the source directory"
                                             : "trusted glTF resource exceeds the bounded import budget";
        if (error) *error = "trusted glTF resource is unavailable or exceeds the import budget";
        return false;
    }
    *bytes = static_cast<size_t>(size);
    return true;
}

tinygltf::FsCallbacks confinedTrustedGltfAccess(TrustedGltfReadContext& context) {
    tinygltf::FsCallbacks callbacks;
    callbacks.FileExists = trustedFileExists;
    callbacks.ExpandFilePath = [](const std::string& path, void*) { return path; };
    callbacks.ReadWholeFile = trustedReadWholeFile;
    callbacks.WriteWholeFile = [](std::string* error, const std::string&,
                                  const std::vector<unsigned char>&, void*) {
        if (error) *error = "write is disabled during trusted glTF import";
        return false;
    };
    callbacks.GetFileSizeInBytes = trustedGetFileSize;
    callbacks.user_data = &context;
    return callbacks;
}

float finite(float value, const std::filesystem::path& path, const char* field) {
    if (!std::isfinite(value)) importError(path, std::string("non-finite ") + field);
    return value;
}

std::array<float, 3> vec3(const std::vector<double>& values, std::array<float, 3> fallback,
                           const std::filesystem::path& path, const char* field) {
    if (values.empty()) return fallback;
    if (values.size() != 3) importError(path, std::string(field) + " must have three components");
    return {finite(static_cast<float>(values[0]), path, field),
            finite(static_cast<float>(values[1]), path, field),
            finite(static_cast<float>(values[2]), path, field)};
}

struct Mat4 {
    std::array<double, 16> value{};

    static Mat4 identity() {
        Mat4 matrix;
        matrix.value = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        return matrix;
    }

    static Mat4 fromNode(const tinygltf::Node& node, const std::filesystem::path& path) {
        if (!node.matrix.empty()) {
            if (node.matrix.size() != 16) importError(path, "node matrix must have 16 components");
            Mat4 matrix;
            for (size_t i = 0; i < 16; ++i) {
                if (!std::isfinite(node.matrix[i])) importError(path, "node matrix is non-finite");
                matrix.value[i] = node.matrix[i];
            }
            return matrix;
        }
        const auto t = vec3(node.translation, {0,0,0}, path, "node translation");
        const auto s = vec3(node.scale, {1,1,1}, path, "node scale");
        std::array<double, 4> q{0,0,0,1};
        if (!node.rotation.empty()) {
            if (node.rotation.size() != 4) importError(path, "node rotation must have four components");
            for (size_t i = 0; i < 4; ++i) {
                if (!std::isfinite(node.rotation[i])) importError(path, "node rotation is non-finite");
                q[i] = node.rotation[i];
            }
            const double length = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
            if (length < 1e-12) importError(path, "node rotation has zero length");
            for (double& component : q) component /= length;
        }
        const double x = q[0], y = q[1], z = q[2], w = q[3];
        Mat4 matrix = identity();
        matrix.value = {
            (1-2*(y*y+z*z))*s[0], (2*(x*y+z*w))*s[0],     (2*(x*z-y*w))*s[0],     0,
            (2*(x*y-z*w))*s[1],     (1-2*(x*x+z*z))*s[1], (2*(y*z+x*w))*s[1],     0,
            (2*(x*z+y*w))*s[2],     (2*(y*z-x*w))*s[2],     (1-2*(x*x+y*y))*s[2], 0,
            t[0], t[1], t[2], 1,
        };
        return matrix;
    }

    static Mat4 multiply(const Mat4& left, const Mat4& right) {
        Mat4 result{};
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                for (int inner = 0; inner < 4; ++inner)
                    result.value[column * 4 + row] += left.value[inner * 4 + row] *
                                                       right.value[column * 4 + inner];
            }
        }
        return result;
    }

    std::array<float, 3> point(std::array<float, 3> point) const {
        return {static_cast<float>(value[0]*point[0] + value[4]*point[1] + value[8]*point[2] + value[12]),
                static_cast<float>(value[1]*point[0] + value[5]*point[1] + value[9]*point[2] + value[13]),
                static_cast<float>(value[2]*point[0] + value[6]*point[1] + value[10]*point[2] + value[14])};
    }
};

std::array<float, 3> quaternionToEulerDegrees(double x, double y, double z, double w) {
    const double sinX = 2.0 * (w*x + y*z);
    const double cosX = 1.0 - 2.0 * (x*x + y*y);
    const double sinY = std::clamp(2.0 * (w*y - z*x), -1.0, 1.0);
    const double sinZ = 2.0 * (w*z + x*y);
    const double cosZ = 1.0 - 2.0 * (y*y + z*z);
    constexpr double toDegrees = 180.0 / std::numbers::pi;
    return {static_cast<float>(std::atan2(sinX, cosX) * toDegrees),
            static_cast<float>(std::asin(sinY) * toDegrees),
            static_cast<float>(std::atan2(sinZ, cosZ) * toDegrees)};
}

MeshCraft::Mc3::Mc3Transform transformFromMatrix(const Mat4& matrix,
                                                  const std::filesystem::path& path) {
    auto length = [](double x, double y, double z) { return std::sqrt(x*x + y*y + z*z); };
    const double sx = length(matrix.value[0], matrix.value[1], matrix.value[2]);
    const double sy = length(matrix.value[4], matrix.value[5], matrix.value[6]);
    const double sz = length(matrix.value[8], matrix.value[9], matrix.value[10]);
    if (sx < 1e-10 || sy < 1e-10 || sz < 1e-10) importError(path, "node transform has a zero scale axis");
    const double m00 = matrix.value[0] / sx, m01 = matrix.value[4] / sy, m02 = matrix.value[8] / sz;
    const double m10 = matrix.value[1] / sx, m11 = matrix.value[5] / sy, m12 = matrix.value[9] / sz;
    const double m20 = matrix.value[2] / sx, m21 = matrix.value[6] / sy, m22 = matrix.value[10] / sz;
    const double trace = m00 + m11 + m22;
    double x, y, z, w;
    if (trace > 0.0) {
        const double root = std::sqrt(trace + 1.0) * 2.0;
        w = 0.25 * root; x = (m21 - m12) / root; y = (m02 - m20) / root; z = (m10 - m01) / root;
    } else if (m00 > m11 && m00 > m22) {
        const double root = std::sqrt(1.0 + m00 - m11 - m22) * 2.0;
        w = (m21 - m12) / root; x = 0.25 * root; y = (m01 + m10) / root; z = (m02 + m20) / root;
    } else if (m11 > m22) {
        const double root = std::sqrt(1.0 + m11 - m00 - m22) * 2.0;
        w = (m02 - m20) / root; x = (m01 + m10) / root; y = 0.25 * root; z = (m12 + m21) / root;
    } else {
        const double root = std::sqrt(1.0 + m22 - m00 - m11) * 2.0;
        w = (m10 - m01) / root; x = (m02 + m20) / root; y = (m12 + m21) / root; z = 0.25 * root;
    }
    const double qLength = std::sqrt(x*x + y*y + z*z + w*w);
    if (qLength < 1e-10) importError(path, "node transform has an invalid rotation");
    x /= qLength; y /= qLength; z /= qLength; w /= qLength;
    MeshCraft::Mc3::Mc3Transform transform;
    transform.position = {finite(static_cast<float>(matrix.value[12]), path, "node translation"),
                          finite(static_cast<float>(matrix.value[13]), path, "node translation"),
                          finite(static_cast<float>(matrix.value[14]), path, "node translation")};
    transform.scale = {finite(static_cast<float>(sx), path, "node scale"),
                       finite(static_cast<float>(sy), path, "node scale"),
                       finite(static_cast<float>(sz), path, "node scale")};
    transform.rotation = quaternionToEulerDegrees(x, y, z, w);
    return transform;
}

std::string wrapName(int wrap) {
    if (wrap == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE) return "clamp";
    if (wrap == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT) return "mirror";
    return "repeat";
}

std::array<float, 3> colorFromValue(const tinygltf::Value& value,
                                    std::array<float, 3> fallback) {
    if (!value.IsArray() || value.ArrayLen() != 3) return fallback;
    return {static_cast<float>(value.Get(0).GetNumberAsDouble()),
            static_cast<float>(value.Get(1).GetNumberAsDouble()),
            static_cast<float>(value.Get(2).GetNumberAsDouble())};
}

float clampedFinite(double value, float minimum, float maximum,
                    const std::filesystem::path& path, const char* field) {
    return std::clamp(finite(static_cast<float>(value), path, field), minimum, maximum);
}

} // namespace

GltfImportResult importSelfContainedGlb(const std::filesystem::path& path) {
    if (lowerExtension(path) != ".glb")
        importError(path, "only self-contained .glb input is accepted (external .gltf is not trusted by this route)");
    std::error_code errorCode;
    const uintmax_t fileSize = std::filesystem::file_size(path, errorCode);
    if (errorCode) importError(path, "cannot inspect input file");
    if (fileSize == 0 || fileSize > kMaxImportGlbBytes)
        importError(path, "input exceeds the 48 MiB inline-embed safety limit");
    std::ifstream stream(path, std::ios::binary);
    if (!stream) importError(path, "cannot open input file");
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), {});
    if (bytes.size() != fileSize) importError(path, "could not read the complete input file");

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(preserveImageBytes, nullptr);
    std::string callbackError;
    if (!loader.SetFsCallbacks(denyExternalFileAccess(), &callbackError))
        importError(path, "cannot configure external-resource denial: " + callbackError);
    tinygltf::Model model;
    std::string error, warning;
    if (!loader.LoadBinaryFromMemory(&model, &error, &warning, bytes.data(),
                                     static_cast<unsigned int>(bytes.size()), ""))
        importError(path, error.empty() ? "tinygltf rejected the GLB" : error);
    for (const auto& buffer : model.buffers)
        if (!buffer.uri.empty()) importError(path, "GLB references an external buffer");
    for (const auto& image : model.images)
        if (!image.uri.empty() && image.uri.rfind("data:", 0) != 0)
            importError(path, "GLB references an external image");
    if (model.scenes.empty()) importError(path, "GLB has no scene");
    const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(model.scenes.size()))
        importError(path, "GLB default scene index is invalid");
    if (model.nodes.size() > static_cast<size_t>(kMaxImportNodes))
        importError(path, "GLB exceeds the 4096-node import limit");

    GltfImportResult result;
    const std::string prefix = safeToken(path.stem().string(), "glb");
    const std::string embedId = prefix + "_source";
    result.document.model = path.stem().string();
    result.document.embeds[embedId] = {embedId, "", base64Encode(bytes)};
    if (!warning.empty()) result.warnings.push_back("GLB parser: " + warning);
    if (!model.animations.empty()) result.warnings.push_back("GLB animations are not imported");

    std::vector<std::string> textureIds(model.textures.size());
    for (size_t textureIndex = 0; textureIndex < model.textures.size(); ++textureIndex) {
        const auto& texture = model.textures[textureIndex];
        if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size())) {
            result.warnings.push_back("GLB texture " + std::to_string(textureIndex) + " has no importable image");
            continue;
        }
        const auto& image = model.images[static_cast<size_t>(texture.source)];
        if (image.image.empty()) {
            result.warnings.push_back("GLB image " + std::to_string(texture.source) + " has no embedded bytes");
            continue;
        }
        const std::string textureId = prefix + "_image" + std::to_string(texture.source);
        if (!textureIds[textureIndex].empty()) continue;
        Mc3Texture imported(textureId, "data:" +
            (image.mimeType.empty() ? std::string("application/octet-stream") : image.mimeType) +
            ";base64," + base64Encode(image.image));
        if (texture.sampler >= 0 && texture.sampler < static_cast<int>(model.samplers.size())) {
            const auto& sampler = model.samplers[static_cast<size_t>(texture.sampler)];
            imported.wrapU = wrapName(sampler.wrapS);
            imported.wrapV = wrapName(sampler.wrapT);
            imported.filter = sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ? "nearest" : "linear";
            imported.mipMaps = sampler.minFilter != TINYGLTF_TEXTURE_FILTER_NEAREST &&
                               sampler.minFilter != TINYGLTF_TEXTURE_FILTER_LINEAR;
        }
        result.document.textures[textureId] = std::move(imported);
        textureIds[textureIndex] = textureId;
    }

    auto textureIdFor = [&](int index) -> std::string {
        return index >= 0 && index < static_cast<int>(textureIds.size()) ? textureIds[static_cast<size_t>(index)]
                                                                          : std::string();
    };
    std::vector<std::string> materialIds(model.materials.size());
    for (size_t materialIndex = 0; materialIndex < model.materials.size(); ++materialIndex) {
        const auto& source = model.materials[materialIndex];
        const std::string id = prefix + "_material" + std::to_string(materialIndex);
        Mc3Material material;
        material.name = id;
        if (source.pbrMetallicRoughness.baseColorFactor.size() == 4) {
            material.baseColor = {clampedFinite(source.pbrMetallicRoughness.baseColorFactor[0], 0.0f, 1.0f, path, "base color"),
                                  clampedFinite(source.pbrMetallicRoughness.baseColorFactor[1], 0.0f, 1.0f, path, "base color"),
                                  clampedFinite(source.pbrMetallicRoughness.baseColorFactor[2], 0.0f, 1.0f, path, "base color"),
                                  clampedFinite(source.pbrMetallicRoughness.baseColorFactor[3], 0.0f, 1.0f, path, "base color")};
        }
        material.metallic = clampedFinite(source.pbrMetallicRoughness.metallicFactor, 0.0f, 1.0f, path, "metallic factor");
        material.roughness = clampedFinite(source.pbrMetallicRoughness.roughnessFactor, 0.0f, 1.0f, path, "roughness factor");
        if (source.emissiveFactor.size() == 3)
            material.emissiveColor = {clampedFinite(source.emissiveFactor[0], 0.0f, 1.0f, path, "emissive factor"),
                                      clampedFinite(source.emissiveFactor[1], 0.0f, 1.0f, path, "emissive factor"),
                                      clampedFinite(source.emissiveFactor[2], 0.0f, 1.0f, path, "emissive factor")};
        material.alphaMode = source.alphaMode == "BLEND" ? "blend" : source.alphaMode == "MASK" ? "mask" : "opaque";
        material.alphaCutoff = clampedFinite(source.alphaCutoff, 0.0f, 1.0f, path, "alpha cutoff");
        material.doubleSided = source.doubleSided;
        material.baseColorTexture = textureIdFor(source.pbrMetallicRoughness.baseColorTexture.index);
        material.metallicRoughnessTexture = textureIdFor(source.pbrMetallicRoughness.metallicRoughnessTexture.index);
        material.normalTexture = textureIdFor(source.normalTexture.index);
        material.normalScale = clampedFinite(source.normalTexture.scale, 0.0f, 1.0f, path, "normal scale");
        material.occlusionTexture = textureIdFor(source.occlusionTexture.index);
        material.occlusionStrength = clampedFinite(source.occlusionTexture.strength, 0.0f, 1.0f, path, "occlusion strength");
        material.emissiveTexture = textureIdFor(source.emissiveTexture.index);
        result.document.materials[id] = std::move(material);
        materialIds[materialIndex] = id;
    }

    // Validate all active triangle geometry through the same hardened embedded
    // loader used at export/viewport time before mutating a caller's document.
    const auto verified = loadEmbeddedGltfMesh({}, result.document.embeds.at(embedId));
    result.triangleCount = static_cast<int>(verified.indices.size() / 3);

    std::vector<std::optional<Mat4>> world(model.nodes.size());
    std::vector<bool> active(model.nodes.size(), false);
    std::function<std::shared_ptr<Mc3Object>(int, const Mat4&, int)> importNode;
    importNode = [&](int nodeIndex, const Mat4& parent, int depth) -> std::shared_ptr<Mc3Object> {
        if (depth > kMaxImportDepth) importError(path, "scene hierarchy exceeds depth 64");
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size()))
            importError(path, "scene references an invalid node");
        if (active[static_cast<size_t>(nodeIndex)]) importError(path, "scene hierarchy contains a cycle");
        active[static_cast<size_t>(nodeIndex)] = true;
        const auto& source = model.nodes[static_cast<size_t>(nodeIndex)];
        const Mat4 local = Mat4::fromNode(source, path);
        const Mat4 nodeWorld = Mat4::multiply(parent, local);
        world[static_cast<size_t>(nodeIndex)] = nodeWorld;
        auto object = std::make_shared<Mc3Object>();
        object->type = ObjectType::Group;
        object->name = source.name.empty() ? "Node" + std::to_string(nodeIndex) : source.name;
        object->id = prefix + "_node" + std::to_string(nodeIndex);
        object->transform = transformFromMatrix(local, path);
        if (source.skin >= 0)
            result.warnings.push_back("GLB node '" + object->name + "' uses a skin; its mesh is not imported");
        if (source.mesh >= 0 && source.skin < 0) {
            if (source.mesh >= static_cast<int>(model.meshes.size())) importError(path, "node references an invalid mesh");
            const auto& mesh = model.meshes[static_cast<size_t>(source.mesh)];
            for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex) {
                const auto& primitive = mesh.primitives[primitiveIndex];
                if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                    result.warnings.push_back("GLB node '" + object->name + "' primitive " +
                                              std::to_string(primitiveIndex) + " is not triangles and was skipped");
                    continue;
                }
                if (!primitive.targets.empty()) {
                    result.warnings.push_back("GLB node '" + object->name + "' primitive " +
                                              std::to_string(primitiveIndex) + " uses morph targets and was skipped");
                    continue;
                }
                auto child = Mc3Object::makeMesh(object->name + "_primitive" + std::to_string(primitiveIndex),
                                                  "embed:" + embedId);
                child->id = prefix + "_node" + std::to_string(nodeIndex) + "_primitive" +
                            std::to_string(primitiveIndex);
                child->metadata[std::string(kGltfMeshIndexMetadataKey)] = std::to_string(source.mesh);
                child->metadata[std::string(kGltfPrimitiveIndexMetadataKey)] = std::to_string(primitiveIndex);
                if (primitive.material >= 0 && primitive.material < static_cast<int>(materialIds.size()))
                    child->material = materialIds[static_cast<size_t>(primitive.material)];
                else if (primitive.material >= 0)
                    result.warnings.push_back("GLB primitive references an unavailable material");
                object->children.push_back(std::move(child));
            }
        }
        for (int childIndex : source.children)
            object->children.push_back(importNode(childIndex, nodeWorld, depth + 1));
        active[static_cast<size_t>(nodeIndex)] = false;
        return object;
    };
    const Mat4 identity = Mat4::identity();
    for (int root : model.scenes[static_cast<size_t>(sceneIndex)].nodes)
        result.document.objects.push_back(importNode(root, identity, 0));
    if (result.document.objects.empty()) importError(path, "default scene has no nodes");

    for (size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex) {
        if (!world[nodeIndex]) continue; // only the imported default scene
        const auto& node = model.nodes[nodeIndex];
        const auto transform = transformFromMatrix(*world[nodeIndex], path);
        if (node.camera >= 0) {
            if (node.camera >= static_cast<int>(model.cameras.size())) importError(path, "node references an invalid camera");
            const auto& source = model.cameras[static_cast<size_t>(node.camera)];
            Mc3Camera camera;
            camera.name = source.name.empty() ? "Camera" + std::to_string(node.camera) : source.name;
            camera.position = transform.position;
            camera.rotation = transform.rotation;
            if (source.type == "orthographic") {
                camera.type = CameraType::Orthographic;
                camera.orthoSize = static_cast<float>(source.orthographic.ymag);
                camera.orthoAspect = source.orthographic.ymag != 0.0
                    ? static_cast<float>(source.orthographic.xmag / source.orthographic.ymag) : 1.0f;
                camera.nearPlane = static_cast<float>(source.orthographic.znear);
                camera.farPlane = static_cast<float>(source.orthographic.zfar);
            } else {
                camera.type = CameraType::Perspective;
                camera.fov = static_cast<float>(source.perspective.yfov * 180.0 / std::numbers::pi);
                camera.nearPlane = static_cast<float>(source.perspective.znear);
                camera.farPlane = source.perspective.zfar > 0.0 ? static_cast<float>(source.perspective.zfar) : 1000.0f;
            }
            result.document.cameras.push_back(std::move(camera));
        }
    }

    if (!model.lights.empty()) {
        // Tinygltf parses KHR_lights_punctual into these typed fields. Prefer
        // them over the generic extension map: its own writer serializes the
        // same extension from model.lights/node.light, so this is the normal
        // round-trip path as well as the least error-prone reader path.
        for (size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex) {
            if (!world[nodeIndex]) continue;
            const auto& node = model.nodes[nodeIndex];
            if (node.light < 0) continue;
            if (node.light >= static_cast<int>(model.lights.size())) {
                result.warnings.push_back("GLB node references an invalid punctual light");
                continue;
            }
            const auto& source = model.lights[static_cast<size_t>(node.light)];
            Mc3Light light;
            light.name = source.name.empty() ? "Light" + std::to_string(node.light) : source.name;
            light.type = source.type == "directional" ? LightType::Directional :
                         source.type == "spot" ? LightType::Spot :
                         source.type == "point" ? LightType::Point : LightType::Ambient;
            if (light.type == LightType::Ambient) {
                result.warnings.push_back("GLB punctual light '" + light.name +
                                          "' has unsupported type '" + source.type + "'");
                continue;
            }
            light.color = vec3(source.color, {1,1,1}, path, "punctual light color");
            light.brightness = finite(static_cast<float>(source.intensity), path, "punctual light intensity");
            light.range = finite(static_cast<float>(source.range), path, "punctual light range");
            const auto position = world[nodeIndex]->point({0,0,0});
            const auto forwardPoint = world[nodeIndex]->point({0,0,-1});
            light.position = position;
            light.direction = {forwardPoint[0] - position[0], forwardPoint[1] - position[1],
                               forwardPoint[2] - position[2]};
            const float length = std::sqrt(light.direction[0]*light.direction[0] +
                                           light.direction[1]*light.direction[1] +
                                           light.direction[2]*light.direction[2]);
            if (length > 1e-6f)
                for (float& component : light.direction) component /= length;
            if (light.type == LightType::Spot) {
                const float outer = finite(static_cast<float>(source.spot.outerConeAngle), path,
                                           "spot outer cone angle");
                const float inner = finite(static_cast<float>(source.spot.innerConeAngle), path,
                                           "spot inner cone angle");
                light.angle = outer * 180.0f / std::numbers::pi_v<float>;
                light.falloff = outer > 0.0f ? std::clamp(1.0f - inner / outer, 0.0f, 1.0f) : 0.0f;
            }
            result.document.lights.push_back(std::move(light));
        }
    } else {
    const auto lightsExtension = model.extensions.find("KHR_lights_punctual");
    if (lightsExtension != model.extensions.end() && lightsExtension->second.IsObject() &&
        lightsExtension->second.Has("lights")) {
        const auto& lights = lightsExtension->second.Get("lights");
        if (lights.IsArray()) {
            for (size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex) {
                if (!world[nodeIndex]) continue;
                const auto extension = model.nodes[nodeIndex].extensions.find("KHR_lights_punctual");
                if (extension == model.nodes[nodeIndex].extensions.end() || !extension->second.IsObject() ||
                    !extension->second.Has("light")) continue;
                const int lightIndex = extension->second.Get("light").GetNumberAsInt();
                if (lightIndex < 0 || lightIndex >= static_cast<int>(lights.ArrayLen())) {
                    result.warnings.push_back("GLB node references an invalid punctual light");
                    continue;
                }
                const auto& source = lights.Get(static_cast<size_t>(lightIndex));
                if (!source.IsObject() || !source.Has("type")) {
                    result.warnings.push_back("GLB punctual light has no supported type");
                    continue;
                }
                Mc3Light light;
                light.name = source.Has("name") ? source.Get("name").Get<std::string>() :
                             "Light" + std::to_string(lightIndex);
                const std::string type = source.Get("type").Get<std::string>();
                light.type = type == "directional" ? LightType::Directional :
                             type == "spot" ? LightType::Spot : LightType::Point;
                if (type != "directional" && type != "spot" && type != "point") {
                    result.warnings.push_back("GLB punctual light '" + light.name + "' has unsupported type '" + type + "'");
                    continue;
                }
                light.color = source.Has("color") ? colorFromValue(source.Get("color"), {1,1,1}) : std::array<float,3>{1,1,1};
                light.brightness = source.Has("intensity") ?
                    static_cast<float>(source.Get("intensity").GetNumberAsDouble()) : 1.0f;
                if (source.Has("range")) light.range = static_cast<float>(source.Get("range").GetNumberAsDouble());
                const auto position = world[nodeIndex]->point({0,0,0});
                const auto forwardPoint = world[nodeIndex]->point({0,0,-1});
                light.position = position;
                light.direction = {forwardPoint[0] - position[0], forwardPoint[1] - position[1],
                                   forwardPoint[2] - position[2]};
                const float length = std::sqrt(light.direction[0]*light.direction[0] +
                                               light.direction[1]*light.direction[1] +
                                               light.direction[2]*light.direction[2]);
                if (length > 1e-6f)
                    for (float& component : light.direction) component /= length;
                if (light.type == LightType::Spot && source.Has("spot")) {
                    const auto& spot = source.Get("spot");
                    if (spot.IsObject() && spot.Has("outerConeAngle")) {
                        const float outer = static_cast<float>(spot.Get("outerConeAngle").GetNumberAsDouble());
                        const float inner = spot.Has("innerConeAngle")
                            ? static_cast<float>(spot.Get("innerConeAngle").GetNumberAsDouble()) : 0.0f;
                        light.angle = outer * 180.0f / std::numbers::pi_v<float>;
                        light.falloff = outer > 0.0f ? std::clamp(1.0f - inner / outer, 0.0f, 1.0f) : 0.0f;
                    }
                }
                result.document.lights.push_back(std::move(light));
            }
        }
    }
    }
    return result;
}

GltfImportResult importTrustedGltf(const std::filesystem::path& path) {
    if (lowerExtension(path) != ".gltf")
        importError(path, "trusted external-resource import accepts only .gltf input");
    std::error_code errorCode;
    const std::filesystem::path sourcePath = std::filesystem::weakly_canonical(path, errorCode);
    if (errorCode) importError(path, "cannot resolve input file");
    const uintmax_t jsonSize = std::filesystem::file_size(sourcePath, errorCode);
    if (errorCode) importError(path, "cannot inspect input file");
    if (jsonSize == 0 || jsonSize > kMaxTrustedGltfJsonBytes)
        importError(path, "JSON source exceeds the 16 MiB trusted-import limit");

    // This route intentionally permits declared external buffers/images, but
    // only after the UI's explicit trust opt-in and only inside the selected
    // .gltf's own directory. We immediately turn the resolved model into a
    // bounded inline GLB, so the saved MC3 never retains those paths.
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(preserveImageBytes, nullptr);
    TrustedGltfReadContext readContext{sourcePath.parent_path(), 0, {}};
    std::string callbackError;
    if (!loader.SetFsCallbacks(confinedTrustedGltfAccess(readContext), &callbackError))
        importError(path, "cannot configure trusted resource confinement: " + callbackError);
    tinygltf::Model model;
    std::string error, warning;
    if (!loader.LoadASCIIFromFile(&model, &error, &warning, sourcePath.string()))
        importError(path, !readContext.denial.empty() ? readContext.denial :
                          (error.empty() ? "tinygltf rejected the trusted glTF" : error));
    uintmax_t payloadBytes = 0;
    auto addPayload = [&](size_t bytes, const char* kind) {
        if (bytes > kMaxImportGlbBytes || payloadBytes > kMaxImportGlbBytes - bytes)
            importError(path, std::string("trusted ") + kind + " data exceeds the 48 MiB import limit");
        payloadBytes += bytes;
    };
    for (const auto& buffer : model.buffers) addPayload(buffer.data.size(), "buffer");
    for (const auto& image : model.images) addPayload(image.image.size(), "image");

    // Tinygltf preserves the source URI after loading its bytes. A GLB has
    // no legal external buffer/image URI, so clear only those now-resolved
    // references before serializing the trusted model into our portable
    // inline representation.
    for (auto& buffer : model.buffers) buffer.uri.clear();
    for (auto& image : model.images) {
        if (!image.image.empty()) {
            image.uri.clear();
            image.as_is = true;
        }
    }

    std::filesystem::path temporary;
    try {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        temporary = std::filesystem::temp_directory_path() /
                    ("meshcraft-trusted-import-" + std::to_string(nonce) + ".glb");
        tinygltf::TinyGLTF writer;
        if (!writer.WriteGltfSceneToFile(&model, temporary.string(), true, true, false, true))
            importError(path, "could not convert trusted glTF to a self-contained GLB");
        auto result = importSelfContainedGlb(temporary);
        result.document.model = path.stem().string();
        result.warnings.insert(result.warnings.begin(),
                               "trusted external .gltf resources were read once and embedded inline");
        if (!warning.empty()) result.warnings.insert(result.warnings.begin() + 1, "glTF parser: " + warning);
        std::filesystem::remove(temporary, errorCode);
        return result;
    } catch (...) {
        if (!temporary.empty()) std::filesystem::remove(temporary, errorCode);
        throw;
    }
}

} // namespace mc3togltf
