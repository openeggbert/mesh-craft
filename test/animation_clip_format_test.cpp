// SYS-W14-38: named clip ranges are MC3 data, not editor-only state. Verify
// XML, JSON, and tagged MCB all retain every preview/export-relevant field.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mcb/McbReader.hpp>
#include <MeshCraft/Mcb/McbWriter.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace MeshCraft::Mc3;

static int failures = 0;

static void check(bool condition, const std::string& message) {
    if (condition) std::cout << "PASS: " << message << '\n';
    else { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

static bool near(float actual, float expected) {
    return std::fabs(actual - expected) < 1e-6f;
}

static void checkClips(const Mc3Document& doc, const std::string& label) {
    const auto actionIt = doc.actions.find("Motion");
    check(actionIt != doc.actions.end(), label + ": action survives");
    if (actionIt == doc.actions.end()) return;
    const auto& clips = actionIt->second.clips;
    check(clips.size() == 2, label + ": two named clips survive");
    if (clips.size() != 2) return;
    const auto& forward = clips[0];
    const auto& reverse = clips[1];
    check(forward.name == "Forward" && near(forward.startTime, 0.5f) && near(forward.endTime, 1.5f) &&
          near(forward.playbackRate, 0.75f) && forward.loop && !forward.reverse &&
          near(forward.transitionDuration, 0.2f),
          label + ": forward range/rate/loop/transition survive");
    check(reverse.name == "Reverse" && near(reverse.startTime, 1.5f) && near(reverse.endTime, 3.0f) &&
          near(reverse.playbackRate, 1.5f) && !reverse.loop && reverse.reverse &&
          near(reverse.transitionDuration, 0.4f),
          label + ": reverse range/rate/direction/transition survive");
}

int main() {
    const fs::path root = fs::temp_directory_path() / "meshcraft_animation_clip_format_test";
    const fs::path xml = root / "clips.mc3.xml";
    const fs::path json = root / "clips.mc3.json";
    const fs::path mcb = root / "clips.mcb";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root, error);

    Mc3Document doc;
    Mc3Action action = Mc3Action::make("Motion", 3.0f, true);
    action.clips = {
        {"Forward", 0.5f, 1.5f, 0.75f, true, false, 0.2f},
        {"Reverse", 1.5f, 3.0f, 1.5f, false, true, 0.4f},
    };
    doc.actions[action.name] = action;

    try {
        doc.saveToFile(xml);
        checkClips(Mc3Document::loadFromFile(xml), "XML");
        doc.saveToJsonFile(json);
        checkClips(Mc3Document::loadFromJsonFile(json), "JSON");
        MeshCraft::Mcb::saveToFile(doc, mcb);
        checkClips(MeshCraft::Mcb::loadFromFile(mcb), "MCB");
    } catch (const std::exception& exception) {
        check(false, std::string("clip format round-trip: ") + exception.what());
    }

    fs::remove_all(root, error);
    if (failures == 0) {
        std::cout << "All animation clip format checks passed.\n";
        return 0;
    }
    std::cerr << failures << " animation clip format check(s) FAILED.\n";
    return 1;
}
