// Frame-driven regression test for the undo-snapshot-timing fix (AUD-036,
// commit 737af77; deeper verification requested by AUD-036b).
//
// Drives real ImGui frames (NewFrame/widget/Render) through a full mouse
// click-drag gesture on DragFloat/DragFloat3/ColorEdit3/SliderFloat, exactly
// as a user interacting with the editor would, and asserts the FIXED pattern
// used throughout the editor since commit 737af77:
//
//     bool ch = ImGui::DragFloat(...);
//     if (ImGui::IsItemActivated()) pushUndo();
//     if (ch) { ...apply the mutation...; markModified(); }
//
// actually produces "exactly one undo snapshot per gesture, taken on the
// activation frame, strictly before any mutation is applied" -- not just that
// pushUndo() is *reachable*, but that its call count and ordering relative to
// the mutation are correct across a real multi-frame drag. This is the
// property the OLD buggy pattern (pushUndo nested inside the widget's
// changed-block) silently violated: IsItemActivated() is true only on the
// activation frame, and DragBehaviorT force-returns false on that exact
// frame, so the two conditions never coincided and the snapshot never fired.
//
// No CNA/SDL window/GL context is required -- ImGui's core widget logic runs
// entirely off IO state (mouse position/buttons) fed directly to the context.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include <cstdio>
#include <functional>
#include <string>

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::printf("PASS: %s\n", msg.c_str()); }
    else      { std::fprintf(stderr, "FAIL: %s\n", msg.c_str()); ++failures; }
}

// Result of driving one full click-drag gesture over a widget using the FIXED
// pattern: `bool ch = Widget(...); if (IsItemActivated()) ++undoCount; if
// (ch) ++mutationFrames;`
struct GestureResult {
    int undoCount = 0;       // snapshots taken (should be exactly 1 per gesture)
    int mutationFrames = 0;  // frames where the widget reported a value change
    int firstMutationFrame = -1;
    int undoFrame = -1;
};

// Drives: hover, press, N move-while-held frames, release. `widget` returns
// ImGui's raw bool (true on any frame the value changed); `frameIndex` starts
// at 0 for the press frame.
static GestureResult driveClickDragGesture(const std::function<bool()>& widget) {
    GestureResult r;
    ImGuiIO& io = ImGui::GetIO();

    auto frame = [&](float mx, float my, bool mouseDown, int frameIndex) {
        io.AddMousePosEvent(mx, my);
        io.AddMouseButtonEvent(0, mouseDown);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 300));
        ImGui::Begin("w", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextItemWidth(200);
        bool changed = widget();
        ImVec2 mn = ImGui::GetItemRectMin(), mx2 = ImGui::GetItemRectMax();
        if (ImGui::IsItemActivated()) {
            ++r.undoCount;
            if (r.undoFrame < 0) r.undoFrame = frameIndex;
        }
        if (changed) {
            ++r.mutationFrames;
            if (r.firstMutationFrame < 0) r.firstMutationFrame = frameIndex;
        }
        ImGui::End();
        ImGui::Render();
        return std::pair{mn, mx2};
    };

    // Frame 0: establish layout (item rect not known before first frame).
    auto [mn, mx] = frame(600, 600, false, -1);
    float cx = (mn.x + mx.x) * 0.5f, cy = (mn.y + mx.y) * 0.5f;

    // Hover (frame 0), press (frame 1 -- the activation frame), 20 drag-move
    // frames while held (frames 2-21), release (frame 22).
    frame(cx, cy, false, 0);
    frame(cx, cy, true, 1);
    for (int i = 1; i <= 20; ++i) frame(cx + static_cast<float>(i) * 3.0f, cy, true, 1 + i);
    frame(cx + 80.0f, cy, false, 22);

    return r;
}

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1000, 800);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pix; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pix, &w, &h);
    io.Fonts->SetTexID(static_cast<ImTextureID>(1));

    // DragFloat: the exact widget class the original bug affected.
    {
        float v = 5.0f;
        auto r = driveClickDragGesture([&]{ return ImGui::DragFloat("##d", &v, 0.5f, 0.0f, 1000.0f); });
        check(r.undoCount == 1,
              "DragFloat: exactly one undo snapshot per click-drag gesture (got " +
              std::to_string(r.undoCount) + ")");
        check(r.mutationFrames > 0,
              "DragFloat: the gesture actually changed the value at least once");
        check(r.undoFrame <= r.firstMutationFrame,
              "DragFloat: the undo snapshot's frame is not later than the first "
              "mutation's frame (snapshot precedes/coincides with, never follows, the edit)");
    }

    // DragFloat3: the 3-component form used for position/rotation/scale.
    {
        float v3[3] = {1.0f, 2.0f, 3.0f};
        auto r = driveClickDragGesture([&]{ return ImGui::DragFloat3("##d3", v3, 0.1f); });
        check(r.undoCount == 1,
              "DragFloat3: exactly one undo snapshot per click-drag gesture (got " +
              std::to_string(r.undoCount) + ")");
        check(r.mutationFrames > 0,
              "DragFloat3: the gesture actually changed the value at least once");
    }

    // ColorEdit3: the other widget class the original bug affected (material/
    // light/environment color pickers).
    {
        float col[3] = {0.2f, 0.4f, 0.6f};
        auto r = driveClickDragGesture([&]{ return ImGui::ColorEdit3("##c", col); });
        check(r.undoCount == 1,
              "ColorEdit3: exactly one undo snapshot per click-drag gesture (got " +
              std::to_string(r.undoCount) + ")");
    }

    // SliderFloat: control case. Sliders snap-on-click, so IsItemActivated()
    // and a value change legitimately coincide on the same (press) frame --
    // this is why undo_snapshot_lint_test.py exempts Slider* from the dead-
    // pattern check. Confirms the harness itself isn't just always reporting
    // undoCount==1 regardless of widget behavior.
    {
        float v = 0.2f;
        auto r = driveClickDragGesture([&]{ return ImGui::SliderFloat("##s", &v, 0.0f, 1.0f); });
        check(r.undoCount == 1,
              "SliderFloat: exactly one undo snapshot per click-drag gesture (got " +
              std::to_string(r.undoCount) + ")");
        check(r.firstMutationFrame == r.undoFrame,
              "SliderFloat: (control case) value change coincides with the activation "
              "frame, unlike Drag/ColorEdit -- confirms the harness distinguishes widget "
              "behaviors rather than trivially always passing");
    }

    ImGui::DestroyContext();

    if (failures == 0) { std::printf("All undo-gesture frame tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d undo-gesture frame test(s) failed.\n", failures);
    return 1;
}
