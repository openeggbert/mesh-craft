// SYS-W14-38: named MC3 clip ranges must become deterministic, playable glTF
// animations, bake reverse/rate without an experimental extension, and report
// omitted non-TRS channels with the original object id.

#include "GltfExporter.hpp"

#include <tiny_gltf.h>

#include <algorithm>
#include <cmath>
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

static bool near(float actual, float expected) {
    return std::abs(actual - expected) < 1e-5f;
}

static std::vector<unsigned char> readBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

static std::vector<float> accessorFloats(const tinygltf::Model& model, int accessorIndex) {
    const auto& accessor = model.accessors.at(accessorIndex);
    const auto& view = model.bufferViews.at(accessor.bufferView);
    const auto& buffer = model.buffers.at(view.buffer);
    const size_t components = tinygltf::GetNumComponentsInType(accessor.type);
    std::vector<float> values(static_cast<size_t>(accessor.count) * components);
    const size_t offset = view.byteOffset + accessor.byteOffset;
    std::memcpy(values.data(), buffer.data.data() + offset, values.size() * sizeof(float));
    return values;
}

static const tinygltf::Animation* findAnimation(const tinygltf::Model& model,
                                                 const std::string& name) {
    const auto it = std::find_if(model.animations.begin(), model.animations.end(),
        [&](const tinygltf::Animation& animation) { return animation.name == name; });
    return it == model.animations.end() ? nullptr : &*it;
}

static const tinygltf::AnimationChannel* translationChannel(const tinygltf::Animation& animation) {
    const auto it = std::find_if(animation.channels.begin(), animation.channels.end(),
        [](const tinygltf::AnimationChannel& channel) { return channel.target_path == "translation"; });
    return it == animation.channels.end() ? nullptr : &*it;
}

int main() {
    const fs::path root = fs::temp_directory_path() / "mc3togltf_animation_clip_export_test";
    const fs::path gltf = root / "clips.gltf";
    const fs::path portable = root / "clips-portable.gltf";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root, error);

    Mc3Document doc;
    doc.model = "clip-export";
    doc.sourcePath = root;
    auto box = Mc3Object::makeBox("Box");
    box->id = "box-id";
    doc.addObject(box);

    Mc3Action action = Mc3Action::make("Motion", 4.0f);
    action.timeScale = 2.0f;
    Mc3Channel position;
    position.targetObject = "Box";
    position.property = AnimatedProperty::PositionX;
    position.keyframes = {
        Mc3Keyframe::linear(0.0f, 0.0f), Mc3Keyframe::linear(2.0f, 2.0f),
        Mc3Keyframe::linear(4.0f, 4.0f)};
    action.channels.push_back(position);
    Mc3Channel visible;
    visible.targetObject = "Box";
    visible.property = AnimatedProperty::Visible;
    visible.keyframes = {Mc3Keyframe::step(0.0f, 1.0f), Mc3Keyframe::step(4.0f, 0.0f)};
    action.channels.push_back(visible);
    action.clips = {
        {"Forward", 1.0f, 3.0f, 0.5f, true, false, 0.25f},
        {"Reverse", 1.0f, 3.0f, 1.0f, false, true, 0.25f},
    };
    doc.actions[action.name] = action;

    try {
        GltfExporter exporter;
        exporter.exportDocument(doc, gltf, OutputFormat::GLTF);
        const bool hasIdReport = std::any_of(exporter.report.begin(), exporter.report.end(),
            [](const ExportReportEntry& entry) {
                return entry.objectId == "box-id" && entry.message.find("visible") != std::string::npos;
            });
        check(hasIdReport, "unsupported non-TRS channel is reported with its MC3 object id");

        tinygltf::TinyGLTF reader;
        tinygltf::Model model;
        std::string loadError, loadWarning;
        check(reader.LoadASCIIFromFile(&model, &loadError, &loadWarning, gltf.string()),
              "clip-exported textual glTF is readable");
        const auto* forward = findAnimation(model, "Motion::Forward");
        const auto* reverse = findAnimation(model, "Motion::Reverse");
        check(forward && reverse && model.animations.size() == 2,
              "each named MC3 clip becomes its own glTF animation");
        if (forward && reverse) {
            const auto* forwardChannel = translationChannel(*forward);
            const auto* reverseChannel = translationChannel(*reverse);
            check(forwardChannel && reverseChannel, "both clip animations retain supported translation channel");
            if (forwardChannel && reverseChannel) {
                const auto& forwardSampler = forward->samplers.at(forwardChannel->sampler);
                const auto& reverseSampler = reverse->samplers.at(reverseChannel->sampler);
                const auto forwardTimes = accessorFloats(model, forwardSampler.input);
                const auto forwardValues = accessorFloats(model, forwardSampler.output);
                const auto reverseTimes = accessorFloats(model, reverseSampler.input);
                const auto reverseValues = accessorFloats(model, reverseSampler.output);
                check(forwardTimes.size() == 3 && near(forwardTimes.front(), 0.0f) &&
                      near(forwardTimes.back(), 2.0f) && forwardValues.size() == 9 &&
                      near(forwardValues.front(), 1.0f) && near(forwardValues[6], 3.0f),
                      "forward clip is boundary-sampled, zero-based, and rate-baked");
                check(reverseTimes.size() == 3 && near(reverseTimes.front(), 0.0f) &&
                      near(reverseTimes.back(), 1.0f) && reverseValues.size() == 9 &&
                      near(reverseValues.front(), 3.0f) && near(reverseValues[6], 1.0f),
                      "reverse clip is baked into increasing glTF time with reversed values");
            }
            check(forward->extras.Get("mc3_clip").Get<std::string>() == "Forward" &&
                  reverse->extras.Get("mc3_clip_reverse").Get<bool>(),
                  "explicit compatibility policy stores clip provenance only in legal extras");
        }

        const std::vector<unsigned char> firstJson = readBytes(gltf);
        const std::vector<unsigned char> firstBinary = readBytes(root / "clips.bin");
        GltfExporter second;
        second.exportDocument(doc, gltf, OutputFormat::GLTF);
        check(firstJson == readBytes(gltf) && firstBinary == readBytes(root / "clips.bin"),
              "clip glTF JSON and binary payload remain deterministic");

        GltfExporter portableExporter;
        portableExporter.animationExportPolicy = AnimationExportPolicy::PortableCoreTrs;
        portableExporter.exportDocument(doc, portable, OutputFormat::GLTF);
        tinygltf::Model portableModel;
        check(reader.LoadASCIIFromFile(&portableModel, &loadError, &loadWarning, portable.string()),
              "portable core-only clip export is readable");
        const auto* portableForward = findAnimation(portableModel, "Motion::Forward");
        check(portableForward && !portableForward->extras.IsObject(),
              "portable policy keeps baked core TRS but omits MC3 extras");
    } catch (const std::exception& exception) {
        check(false, std::string("clip export workflow: ") + exception.what());
    }

    fs::remove_all(root, error);
    if (failures == 0) {
        std::cout << "All animation clip export checks passed.\n";
        return 0;
    }
    std::cerr << failures << " animation clip export check(s) FAILED.\n";
    return 1;
}
