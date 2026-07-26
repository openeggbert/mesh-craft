#include "MeshCraft/AnimationPreviewAlgorithms.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"

#include <cmath>
#include <iostream>
#include <string>

using MeshCraft::AnimationPreviewRange;
using MeshCraft::Mc3::Mc3Action;
using MeshCraft::Mc3::Mc3ActionClip;

static int failures = 0;

static void check(bool condition, const std::string& message) {
    if (condition) std::cout << "PASS: " << message << '\n';
    else { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

static bool near(float actual, float expected) {
    return std::fabs(actual - expected) < 1e-5f;
}

int main() {
    Mc3Action action = Mc3Action::make("Motion", 10.0f, false);
    action.timeScale = 2.0f;
    action.clips.push_back({"Forward", 2.0f, 5.0f, 0.5f, false, false, 0.25f});
    action.clips.push_back({"ReverseLoop", 2.0f, 5.0f, 0.5f, true, true, 0.25f});

    const AnimationPreviewRange full = MeshCraft::animationPreviewRange(action, nullptr);
    bool playing = true;
    check(near(MeshCraft::advanceAnimationPreviewTime(full, 0.0f, 1.0f, playing), 2.0f) && playing,
          "whole action honors its existing timeScale");

    const auto* forwardClip = MeshCraft::findAnimationClip(action, "Forward");
    const AnimationPreviewRange forward = MeshCraft::animationPreviewRange(action, forwardClip);
    check(near(MeshCraft::animationPreviewInitialTime(forward), 2.0f),
          "forward clip starts at its authored range start");
    playing = true;
    check(near(MeshCraft::advanceAnimationPreviewTime(forward, 2.0f, 1.0f, playing), 3.0f) && playing,
          "clip playback rate composes with the action rate");
    playing = true;
    check(near(MeshCraft::advanceAnimationPreviewTime(forward, 4.8f, 0.3f, playing), 5.0f) && !playing,
          "non-looping clip stops at its terminal endpoint");

    const auto* reverseClip = MeshCraft::findAnimationClip(action, "ReverseLoop");
    const AnimationPreviewRange reverse = MeshCraft::animationPreviewRange(action, reverseClip);
    check(near(MeshCraft::animationPreviewInitialTime(reverse), 5.0f),
          "reverse clip starts at its authored range end");
    playing = true;
    check(near(MeshCraft::advanceAnimationPreviewTime(reverse, 5.0f, 1.0f, playing), 4.0f) && playing,
          "reverse clip decrements in action-time while playing");
    check(near(MeshCraft::advanceAnimationPreviewTime(reverse, 2.1f, 0.2f, playing), 4.9f) && playing,
          "reverse loop wraps excess elapsed time deterministically");

    Mc3ActionClip malformed{"Malformed", 10.0f, 10.0f, 0.0f, false, false, 0.0f};
    const AnimationPreviewRange repaired = MeshCraft::animationPreviewRange(action, &malformed);
    check(repaired.endTime > repaired.startTime && repaired.playbackRate > 0.0f,
          "programmatic malformed clip is normalized to a positive range and rate");
    check(MeshCraft::findAnimationClip(action, "missing") == nullptr,
          "unknown clip lookup is explicit rather than falling back silently");

    MeshCraft::AnimOverrideAlg before, after;
    before.position = std::array<float, 3>{0.0f, 2.0f, 4.0f};
    after.position = std::array<float, 3>{8.0f, 10.0f, 12.0f};
    before.visible = false;
    after.visible = true;
    const std::unordered_map<std::string, MeshCraft::AnimOverrideAlg> from{{"Cube", before}};
    const std::unordered_map<std::string, MeshCraft::AnimOverrideAlg> to{{"Cube", after}};
    const auto halfway = MeshCraft::blendAnimOverridesAlg(from, to, 0.5f);
    check(halfway.at("Cube").position && near((*halfway.at("Cube").position)[0], 4.0f) &&
          halfway.at("Cube").visible && *halfway.at("Cube").visible,
          "clip transition cross-fades numeric overrides and switches visibility at midpoint");

    if (failures == 0) {
        std::cout << "All animation preview algorithm checks passed.\n";
        return 0;
    }
    std::cerr << failures << " animation preview algorithm check(s) FAILED.\n";
    return 1;
}
