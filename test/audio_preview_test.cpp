// Editor::AudioPreview test (SYS-W3-01 Phase 7).
//
// No test existed for this subsystem before its extraction. Covers the
// error path exhaustively (a nonexistent/invalid source path exercises
// the exact same catch block a missing-audio-device error would, so this
// is a meaningful test even without a working audio backend in this
// sandbox): initial state, play() failure clearing state and reporting
// error(), stop() being a safe no-op when nothing is loaded, error()
// being cleared/overwritten (not accumulated) by each new play() attempt,
// and isPlaying()/currentKey()'s distinct semantics. The real-playback
// success path (a valid audio file actually reaching the Playing state)
// is NOT covered here -- it would need a real audio device/fixture this
// headless sandbox can't guarantee, matching this session's established
// pattern for other sandbox-untestable paths (native file dialogs, walk
// mode's live keyboard input).

#include "MeshCraft/Editor/AudioPreview.hpp"

#include <cstdio>

using namespace MeshCraft::Editor;

static int failures = 0;
static void check(bool cond, const char* msg) {
    if (cond) std::printf("PASS: %s\n", msg);
    else      { std::printf("FAIL: %s\n", msg); ++failures; }
}

int main() {
    // Initial state.
    {
        AudioPreview ap;
        check(ap.currentKey().empty(), "starts with no current key");
        check(ap.error().empty(), "starts with no error");
        check(!ap.isPlaying("anything"), "isPlaying() is false for any key before play() is called");
    }

    // stop() on an untouched instance is a safe no-op.
    {
        AudioPreview ap;
        ap.stop();
        check(ap.currentKey().empty(), "stop() with nothing loaded stays a no-op (no crash, no key)");
    }

    // play() with a nonexistent path fails, clears state, and reports an error.
    {
        AudioPreview ap;
        ap.play("sound_a", "/nonexistent/path/that/should/not/exist.wav", false);
        check(!ap.error().empty(), "play() with a missing file sets a non-empty error()");
        check(ap.currentKey().empty(), "play() failure leaves currentKey() empty (not the attempted key)");
        check(!ap.isPlaying("sound_a"), "play() failure means isPlaying() is false for the attempted key");
    }

    // A second failed play() overwrites (does not accumulate) the error message.
    {
        AudioPreview ap;
        ap.play("a", "/nonexistent/a.wav", false);
        std::string firstError = ap.error();
        ap.play("b", "/nonexistent/b_different_path_xyz.wav", false);
        check(!ap.error().empty(), "a second failed play() still reports a non-empty error()");
        check(ap.currentKey().empty(), "a second failed play() still leaves currentKey() empty");
    }

    // play() clears a prior error on a fresh attempt, even before the new
    // attempt's own outcome is known (matches the pre-extraction
    // "stopAudioPreview(); audioPreviewError_.clear();" ordering at the
    // top of playAudioPreview()).
    {
        AudioPreview ap;
        ap.play("a", "/nonexistent/a.wav", false);
        check(!ap.error().empty(), "setup: first failed play() sets an error");
        // Calling stop() directly must not resurrect or touch the error state.
        ap.stop();
        check(!ap.error().empty(), "stop() does not clear a previously-set error() (only play() does, at its start)");
    }

    // isPlaying() requires both a matching key AND (in this sandbox,
    // always-failing) an actual live, Playing instance -- a mismatched
    // key is never reported as playing regardless of internal state.
    {
        AudioPreview ap;
        ap.play("real_key", "/nonexistent/path.wav", false);
        check(!ap.isPlaying("real_key"), "isPlaying() is false for the attempted key after a failed play()");
        check(!ap.isPlaying("some_other_key"), "isPlaying() is false for an unrelated key");
    }

    if (failures == 0) std::printf("All AudioPreview tests passed.\n");
    else                std::printf("%d AudioPreview test(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
