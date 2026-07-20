// SYS-W14-19 (2026-07-20) — triggers (doc.triggers, Mc3Trigger: an ordered
// list of {type, ref} steps -- play-action/play-sound/run-script/
// play-music) were fully editable in the "Triggers" tab but nothing
// anywhere ever fired one: no in-scene event system existed, and there
// was no explicit "run this now" action either, so the feature was
// untestable/unusable in the editor despite round-tripping correctly.
//
// Adds a "Fire" action (MeshCraftApplication_UiLeftPanel.cpp's Triggers
// tab: a per-row button plus a prominent "Fire Trigger" button in the
// detail view) that actually executes a trigger's steps: PlayAction sets
// the same currentActionName_/animTime_/animPlaying_ state the Timeline's
// own Play button uses; PlaySound/PlayMusic call the Audio tab's own
// AudioPreview::play(); RunScript reports that the scripting engine
// doesn't exist yet (SYS-W14-18) instead of silently doing nothing.
//
// MeshCraftApplication is CNA-coupled and not headlessly instantiable (no
// test in this repo constructs it -- confirmed, matching this session's
// established pattern for F7/F19/F20's own tests), so this mirrors
// fireTrigger()'s exact control-flow shape against plain Mc3Document data
// and mock playback-state/audio-call trackers instead of the real
// currentActionName_/animPlaying_ members and the real (CNA-coupled)
// AudioPreview class.

#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Trigger.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) std::printf("PASS: %s\n", msg.c_str());
    else      { std::printf("FAIL: %s\n", msg.c_str()); ++failures; }
}

// Mirrors the real fireTrigger()'s mock-able surface: the "current action"
// playback trio (currentActionName_/animTime_/animPlaying_), a recorder
// standing in for audioPreview_.play(key, resolvedSrc, loop) calls, and
// the aggregated status message fireTrigger() builds.
struct MockPlaybackState {
    std::string currentActionName;
    float       animTime = -1.0f;
    bool        animPlaying = false;
    std::vector<std::tuple<std::string, std::string, bool>> audioPlayCalls; // (key, resolvedSrc, loop)
};

static std::string resolveTriggerSrc(const Mc3Document& doc, const std::string& src) {
    if (src.empty()) return src;
    std::filesystem::path p(src);
    return p.is_absolute() ? p.string() : (doc.sourcePath / p).string();
}

// Byte-for-byte mirror of MeshCraftApplication_UiLeftPanel.cpp's
// fireTrigger() lambda, with document_/currentActionName_/animTime_/
// animPlaying_/audioPreview_.play()/setStatusMsg() replaced by their
// mock equivalents above.
static std::string fireTrigger(Mc3Document& doc, MockPlaybackState& state, const Mc3Trigger& trig) {
    int fired = 0, missing = 0, unimplemented = 0;
    for (const auto& step : trig.steps) {
        switch (step.type) {
        case TriggerStepType::PlayAction:
            if (doc.actions.count(step.ref)) {
                state.currentActionName = step.ref;
                state.animTime    = 0.0f;
                state.animPlaying = true;
                ++fired;
            } else ++missing;
            break;
        case TriggerStepType::PlaySound:
            if (doc.sounds.count(step.ref)) {
                const auto& snd = doc.sounds[step.ref];
                state.audioPlayCalls.emplace_back(step.ref, resolveTriggerSrc(doc, snd.src), snd.loop);
                ++fired;
            } else ++missing;
            break;
        case TriggerStepType::PlayMusic:
            if (doc.musicTracks.count(step.ref)) {
                const auto& mus = doc.musicTracks[step.ref];
                state.audioPlayCalls.emplace_back(step.ref, resolveTriggerSrc(doc, mus.src), mus.loop);
                ++fired;
            } else ++missing;
            break;
        case TriggerStepType::RunScript:
            ++unimplemented;
            break;
        }
    }
    std::string msg = "Trigger '" + trig.id + "' fired: " +
        std::to_string(fired) + " step" + (fired == 1 ? "" : "s") + " ran";
    if (missing > 0)
        msg += ", " + std::to_string(missing) + " skipped (ref not found)";
    if (unimplemented > 0)
        msg += ", " + std::to_string(unimplemented) + " skipped (scripting not implemented yet)";
    return msg;
}

int main() {
    // --- PlayAction: valid ref starts playback at the shared "current
    // action" state, exactly like the Timeline's own Play button. ---
    {
        Mc3Document doc;
        doc.actions["Spin"] = Mc3Action{};
        MockPlaybackState state;
        Mc3Trigger trig;
        trig.id = "t1";
        trig.steps.push_back({TriggerStepType::PlayAction, "Spin"});

        std::string msg = fireTrigger(doc, state, trig);
        check(state.currentActionName == "Spin", "PlayAction: currentActionName set to the referenced action");
        check(state.animTime == 0.0f, "PlayAction: animTime reset to 0");
        check(state.animPlaying, "PlayAction: animPlaying set true");
        check(msg.find("1 step ran") != std::string::npos,
              "PlayAction: status message reports 1 step ran (got: " + msg + ")");
    }

    // --- PlayAction: missing ref is skipped, not crashed, and reported. ---
    {
        Mc3Document doc;
        MockPlaybackState state;
        Mc3Trigger trig;
        trig.id = "t2";
        trig.steps.push_back({TriggerStepType::PlayAction, "DoesNotExist"});

        std::string msg = fireTrigger(doc, state, trig);
        check(state.currentActionName.empty(), "PlayAction (missing ref): playback state untouched");
        check(!state.animPlaying, "PlayAction (missing ref): animPlaying stays false");
        check(msg.find("1 skipped (ref not found)") != std::string::npos,
              "PlayAction (missing ref): status message reports the skip (got: " + msg + ")");
    }

    // --- PlaySound: valid ref calls audioPreview_.play() with the
    // resolved src + the sound's own loop flag. ---
    {
        Mc3Document doc;
        doc.sourcePath = "/scenes/level1";
        Mc3Sound snd;
        snd.src = "sfx/click.wav";
        snd.loop = false;
        doc.sounds["click"] = snd;
        MockPlaybackState state;
        Mc3Trigger trig;
        trig.id = "t3";
        trig.steps.push_back({TriggerStepType::PlaySound, "click"});

        std::string msg = fireTrigger(doc, state, trig);
        check(state.audioPlayCalls.size() == 1, "PlaySound: exactly one play call recorded");
        if (!state.audioPlayCalls.empty()) {
            auto& [key, src, loop] = state.audioPlayCalls[0];
            check(key == "click", "PlaySound: play call uses the sound's own key");
            check(src == "/scenes/level1/sfx/click.wav",
                  "PlaySound: relative src resolved against doc.sourcePath (got: " + src + ")");
            check(!loop, "PlaySound: loop flag matches the sound's own (false)");
        }
        check(msg.find("1 step ran") != std::string::npos, "PlaySound: status message reports 1 step ran");
    }

    // --- PlayMusic: same mechanism, independent map, own loop default. ---
    {
        Mc3Document doc;
        doc.sourcePath = "/scenes/level1";
        Mc3Music mus;
        mus.src = "/absolute/path/theme.ogg";
        mus.loop = true;
        doc.musicTracks["theme"] = mus;
        MockPlaybackState state;
        Mc3Trigger trig;
        trig.id = "t4";
        trig.steps.push_back({TriggerStepType::PlayMusic, "theme"});

        fireTrigger(doc, state, trig);
        check(state.audioPlayCalls.size() == 1, "PlayMusic: exactly one play call recorded");
        if (!state.audioPlayCalls.empty()) {
            auto& [key, src, loop] = state.audioPlayCalls[0];
            check(key == "theme", "PlayMusic: play call uses the track's own key");
            check(src == "/absolute/path/theme.ogg",
                  "PlayMusic: an already-absolute src passes through unchanged (got: " + src + ")");
            check(loop, "PlayMusic: loop flag matches the track's own (true)");
        }
    }

    // --- RunScript: reported as unimplemented, not silently dropped, not crashed. ---
    {
        Mc3Document doc;
        MockPlaybackState state;
        Mc3Trigger trig;
        trig.id = "t5";
        trig.steps.push_back({TriggerStepType::RunScript, "some_script"});

        std::string msg = fireTrigger(doc, state, trig);
        check(msg.find("0 step") != std::string::npos, "RunScript: 0 steps actually ran");
        check(msg.find("1 skipped (scripting not implemented yet)") != std::string::npos,
              "RunScript: status message clearly names why it was skipped (got: " + msg + ")");
    }

    // --- Multiple steps: fired/missing/unimplemented counts all aggregate
    // correctly in one status message, matching execution order. ---
    {
        Mc3Document doc;
        doc.actions["Open"] = Mc3Action{};
        doc.sounds["door"] = Mc3Sound{};
        MockPlaybackState state;
        Mc3Trigger trig;
        trig.id = "t6";
        trig.steps.push_back({TriggerStepType::PlayAction, "Open"});
        trig.steps.push_back({TriggerStepType::PlaySound, "door"});
        trig.steps.push_back({TriggerStepType::PlaySound, "missing_sound"});
        trig.steps.push_back({TriggerStepType::RunScript, "unused"});

        std::string msg = fireTrigger(doc, state, trig);
        check(msg.find("2 steps ran") != std::string::npos,
              "Multi-step: 2 steps ran (PlayAction + the valid PlaySound) (got: " + msg + ")");
        check(msg.find("1 skipped (ref not found)") != std::string::npos,
              "Multi-step: 1 skipped for a missing ref (got: " + msg + ")");
        check(msg.find("1 skipped (scripting not implemented yet)") != std::string::npos,
              "Multi-step: 1 skipped for RunScript (got: " + msg + ")");
    }

    if (failures == 0) { std::printf("All trigger-fire tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d trigger-fire test(s) failed.\n", failures);
    return 1;
}
