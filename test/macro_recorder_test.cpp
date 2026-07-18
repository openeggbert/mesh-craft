// Editor::MacroRecorder test (SYS-W3-01 Phase 3).
//
// No test existed for this subsystem before its extraction (new this
// session). Covers: recordStep() is a no-op while not recording, start/
// stop/clear semantics, play() dispatching every verb to the right
// callback with the right arguments (including numeric-arg parsing for
// group_scale/linear_array and the graceful "unknown add type" path),
// playback not re-recording itself, and a real save/load round-trip
// through a temp file (including the empty-path/missing-file error
// messages the pre-extraction saveMacro()/loadMacro() used to report).

#include "MeshCraft/Editor/MacroRecorder.hpp"

#include <cstdio>
#include <filesystem>
#include <sstream>

using namespace MeshCraft::Editor;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const char* msg) {
    if (cond) std::printf("PASS: %s\n", msg);
    else      { std::printf("FAIL: %s\n", msg); ++failures; }
}

namespace {

// Records every callback invocation as one log line, so a single log
// vector can assert both "which callback fired" and "with what argument".
struct Spy {
    std::vector<std::string> log;
    std::vector<std::string> statusMsgs;

    MacroRecorder::Context context() {
        MacroRecorder::Context ctx;
        ctx.addPrimitive = [this](ObjectType t) {
            log.push_back("add:" + std::to_string(static_cast<int>(t)));
        };
        ctx.deleteSelected    = [this] { log.push_back("delete"); };
        ctx.duplicateSelected = [this] { log.push_back("duplicate"); };
        ctx.groupSelected     = [this] { log.push_back("group"); };
        ctx.ungroupSelected   = [this] { log.push_back("ungroup"); };
        ctx.groupScaleSelected = [this](float f) {
            std::ostringstream ss; ss << "group_scale:" << f;
            log.push_back(ss.str());
        };
        ctx.batchRenameSelected = [this](const std::string& name) {
            log.push_back("batch_rename:" + name);
        };
        ctx.linearArrayDuplicate = [this](int count, int axis, float spacing) {
            std::ostringstream ss;
            ss << "linear_array:" << count << ":" << axis << ":" << spacing;
            log.push_back(ss.str());
        };
        ctx.hideSelected    = [this] { log.push_back("hide"); };
        ctx.showAllObjects  = [this] { log.push_back("show_all"); };
        ctx.lockSelected    = [this] { log.push_back("lock"); };
        ctx.unlockSelected  = [this] { log.push_back("unlock"); };
        ctx.setStatusMsg = [this](std::string msg, bool isError, float) {
            statusMsgs.push_back((isError ? "ERR:" : "OK:") + msg);
        };
        return ctx;
    }
};

bool logContains(const std::vector<std::string>& log, const std::string& entry) {
    for (const auto& l : log) if (l == entry) return true;
    return false;
}

bool anyStatusContains(const std::vector<std::string>& msgs, const std::string& needle) {
    for (const auto& m : msgs) if (m.find(needle) != std::string::npos) return true;
    return false;
}

} // namespace

int main() {
    // recordStep() while not recording is a no-op.
    {
        MacroRecorder rec;
        rec.recordStep("delete");
        check(rec.stepCount() == 0, "recordStep() is a no-op while not recording");
    }

    // start/stop/clear.
    {
        MacroRecorder rec;
        check(!rec.isRecording(), "starts out not recording");
        rec.startRecording();
        check(rec.isRecording(), "startRecording() sets isRecording()");
        rec.recordStep("delete");
        rec.recordStep("group");
        check(rec.stepCount() == 2, "recordStep() appends while recording");
        rec.stopRecording();
        check(!rec.isRecording(), "stopRecording() clears isRecording()");
        check(rec.stepCount() == 2, "stopRecording() keeps the recorded steps");
        rec.clear();
        check(rec.stepCount() == 0 && !rec.isRecording(), "clear() empties steps and stops recording");
    }

    // play() dispatches every verb, with correct argument parsing, and does
    // not record the playback itself.
    {
        MacroRecorder rec;
        rec.startRecording();
        rec.recordStep("add", {"Sphere"});
        rec.recordStep("delete");
        rec.recordStep("duplicate");
        rec.recordStep("group");
        rec.recordStep("ungroup");
        rec.recordStep("group_scale", {"2.5"});
        rec.recordStep("batch_rename", {"Prop"});
        rec.recordStep("linear_array", {"5", "1", "2.0"});
        rec.recordStep("hide");
        rec.recordStep("show_all");
        rec.recordStep("lock");
        rec.recordStep("unlock");
        rec.stopRecording();
        check(rec.stepCount() == 12, "all 12 recorded steps are present before playback");

        Spy spy;
        rec.play(spy.context());

        check(logContains(spy.log, "add:" + std::to_string(static_cast<int>(ObjectType::Sphere))),
              "play() dispatches 'add Sphere' to addPrimitive with the right ObjectType");
        check(logContains(spy.log, "delete"), "play() dispatches 'delete'");
        check(logContains(spy.log, "duplicate"), "play() dispatches 'duplicate'");
        check(logContains(spy.log, "group"), "play() dispatches 'group'");
        check(logContains(spy.log, "ungroup"), "play() dispatches 'ungroup'");
        check(logContains(spy.log, "group_scale:2.5"), "play() parses group_scale's float arg");
        check(logContains(spy.log, "batch_rename:Prop"), "play() forwards batch_rename's name arg");
        check(logContains(spy.log, "linear_array:5:1:2"), "play() parses linear_array's count/axis/spacing args");
        check(logContains(spy.log, "hide"), "play() dispatches 'hide'");
        check(logContains(spy.log, "show_all"), "play() dispatches 'show_all'");
        check(logContains(spy.log, "lock"), "play() dispatches 'lock'");
        check(logContains(spy.log, "unlock"), "play() dispatches 'unlock'");
        check(rec.stepCount() == 12, "play() does not mutate the step list");
        check(!rec.isRecording(), "play() leaves isRecording() false (was false before play)");
        check(anyStatusContains(spy.statusMsgs, "replayed"), "play() reports a 'replayed' status message");
    }

    // play() does not record itself even if isRecording() was true beforehand
    // (mirrors the pre-extraction wasRecording save/restore behavior).
    {
        MacroRecorder rec;
        rec.startRecording();
        rec.recordStep("delete");
        // Still "recording" when play() is invoked (e.g. a stray call while
        // the record toggle is on) -- must not append the played-back steps.
        Spy spy;
        rec.play(spy.context());
        check(rec.stepCount() == 1, "play() does not append played-back steps even while isRecording() was true");
        check(rec.isRecording(), "play() restores the prior isRecording() state after playback");
    }

    // Unknown object-type name reports an error instead of crashing or
    // silently falling back (matches Editor::objectTypeFromName()'s own
    // no-silent-fallback contract).
    {
        MacroRecorder rec;
        rec.startRecording();
        rec.recordStep("add", {"NotAType"});
        rec.stopRecording();
        Spy spy;
        rec.play(spy.context());
        check(spy.log.empty(), "unknown 'add' type does not call addPrimitive");
        check(anyStatusContains(spy.statusMsgs, "ERR:Macro: skipped 'add'"),
              "unknown 'add' type reports an error status message");
    }

    // save()/load() round-trip through a real temp file.
    {
        auto path = (std::filesystem::temp_directory_path() / "macro_recorder_test.mc3macro").string();
        std::filesystem::remove(path);

        MacroRecorder rec;
        rec.startRecording();
        rec.recordStep("add", {"Box"});
        rec.recordStep("group_scale", {"1.5"});
        rec.stopRecording();

        Spy saveSpy;
        rec.save(path, saveSpy.context());
        check(std::filesystem::exists(path), "save() writes the macro file");
        check(anyStatusContains(saveSpy.statusMsgs, "Macro saved"), "save() reports a success status message");

        MacroRecorder loaded;
        Spy loadSpy;
        loaded.load(path, loadSpy.context());
        check(loaded.stepCount() == 2, "load() restores the same number of steps");
        check(!loaded.isRecording(), "load() leaves isRecording() false");
        check(anyStatusContains(loadSpy.statusMsgs, "Macro loaded: 2 step(s)"), "load() reports how many steps it loaded");

        Spy playSpy;
        loaded.play(playSpy.context());
        check(logContains(playSpy.log, "add:" + std::to_string(static_cast<int>(ObjectType::Box))),
              "a loaded macro plays back correctly (round-trip through disk preserves verb/args)");
        check(logContains(playSpy.log, "group_scale:1.5"),
              "a loaded macro's numeric arg survives the disk round-trip");

        std::filesystem::remove(path);
    }

    // Error paths: empty path, and loading a file that doesn't exist.
    {
        MacroRecorder rec;
        Spy spy;
        rec.save("", spy.context());
        check(anyStatusContains(spy.statusMsgs, "No path specified"), "save(\"\") reports 'No path specified'");
    }
    {
        MacroRecorder rec;
        Spy spy;
        rec.load("", spy.context());
        check(anyStatusContains(spy.statusMsgs, "No path specified"), "load(\"\") reports 'No path specified'");
    }
    {
        MacroRecorder rec;
        Spy spy;
        rec.load("/nonexistent/path/that/should/not/exist.mc3macro", spy.context());
        check(anyStatusContains(spy.statusMsgs, "ERR:Cannot open"), "load() of a missing file reports 'Cannot open'");
    }

    if (failures == 0) std::printf("All MacroRecorder tests passed.\n");
    else                std::printf("%d MacroRecorder test(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
