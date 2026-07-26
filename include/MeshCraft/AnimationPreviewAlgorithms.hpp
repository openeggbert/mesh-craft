#pragma once

#include <MeshCraft/Mc3/Mc3Animation.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace MeshCraft {

// The normalized action-time range consumed by the editor preview. Keeping
// this separate from the ImGui/application clock makes forward/reverse,
// looping, and malformed-range handling deterministic and unit-testable.
struct AnimationPreviewRange {
    float startTime{0.0f};
    float endTime{1.0f};
    float playbackRate{1.0f};
    bool loop{false};
    bool reverse{false};
};

inline const Mc3::Mc3ActionClip* findAnimationClip(
    const Mc3::Mc3Action& action, const std::string& name)
{
    if (name.empty()) return nullptr;
    for (const auto& clip : action.clips)
        if (clip.name == name) return &clip;
    return nullptr;
}

inline AnimationPreviewRange animationPreviewRange(
    const Mc3::Mc3Action& action, const Mc3::Mc3ActionClip* clip)
{
    constexpr float kMinimumSpan = 1e-4f;
    const float duration = std::max(kMinimumSpan, action.duration);
    if (!clip) return {0.0f, duration, std::max(kMinimumSpan, action.timeScale),
                       action.loop, false};

    float start = std::clamp(clip->startTime, 0.0f, duration);
    float end = std::clamp(clip->endTime, 0.0f, duration);
    if (end - start < kMinimumSpan) {
        if (start >= duration - kMinimumSpan) {
            start = duration - kMinimumSpan;
            end = duration;
        } else {
            end = std::min(duration, start + kMinimumSpan);
        }
    }
    return {start, end,
            std::max(kMinimumSpan, action.timeScale * clip->playbackRate),
            clip->loop, clip->reverse};
}

inline float animationPreviewInitialTime(const AnimationPreviewRange& range) {
    return range.reverse ? range.endTime : range.startTime;
}

// Advances an action-domain cursor. `playing` is set false only for a
// non-looping range that reaches its terminal endpoint. Large frame deltas
// deliberately wrap with fmod so preview behavior is independent of frame
// rate rather than dropping excess elapsed time at a loop boundary.
inline float advanceAnimationPreviewTime(const AnimationPreviewRange& range,
                                         float actionTime, float elapsedSeconds,
                                         bool& playing)
{
    constexpr float kMinimumSpan = 1e-4f;
    const float span = std::max(kMinimumSpan, range.endTime - range.startTime);
    const float clamped = std::clamp(actionTime, range.startTime, range.endTime);
    const float fromStart = range.reverse ? range.endTime - clamped
                                          : clamped - range.startTime;
    const float advanced = fromStart + std::max(0.0f, elapsedSeconds) * range.playbackRate;
    if (advanced >= span) {
        if (!range.loop) {
            playing = false;
            return range.reverse ? range.startTime : range.endTime;
        }
        const float wrapped = std::fmod(advanced, span);
        return range.reverse ? range.endTime - wrapped : range.startTime + wrapped;
    }
    return range.reverse ? range.endTime - advanced : range.startTime + advanced;
}

} // namespace MeshCraft
