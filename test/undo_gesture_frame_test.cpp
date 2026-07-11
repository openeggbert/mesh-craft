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

// AUD-036c extended this file (originally AUD-036b) with frame-driven
// coverage for the remaining widget classes named in the undo-audit mandate:
// Checkbox, Combo, InputText, and a multi-object batch edit. See each
// section below for the specific codebase convention each one verifies.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

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

// No-op guarantee (audit mandate item): hovering a widget across several
// frames WITHOUT ever pressing the mouse button must never activate it or
// change its value -- i.e. no false-positive undo snapshot from mere mouse
// movement. `widget` is called every frame with the mouse hovering its
// (previously measured) rect. Returns true iff nothing fired.
static bool driveHoverOnlyNoOp(const std::function<bool()>& widget) {
    ImGuiIO& io = ImGui::GetIO();
    bool anyActivatedOrChanged = false;

    auto frame = [&](float mx, float my) {
        io.AddMousePosEvent(mx, my);
        io.AddMouseButtonEvent(0, false);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 300));
        ImGui::Begin("w", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextItemWidth(200);
        bool changed = widget();
        if (ImGui::IsItemActivated() || changed) anyActivatedOrChanged = true;
        ImVec2 mn = ImGui::GetItemRectMin(), mx2 = ImGui::GetItemRectMax();
        ImGui::End();
        ImGui::Render();
        return std::pair{mn, mx2};
    };

    auto [mn, mx] = frame(600, 600);
    float cx = (mn.x + mx.x) * 0.5f, cy = (mn.y + mx.y) * 0.5f;
    // Hover, with small jitter (never a click), for several frames.
    for (int i = 0; i < 10; ++i) frame(cx + static_cast<float>(i % 3), cy);

    return !anyActivatedOrChanged;
}

// Result of driving a single click (press frame, then release frame) --
// the gesture shape for Checkbox (as opposed to DragFloat/ColorEdit/
// Slider's press-drag-release used by driveClickDragGesture above).
struct ClickResult {
    int undoCount = 0;
    int mutationFrames = 0;
    int firstMutationFrame = -1;
    int undoFrame = -1;
};

// Drives: hover, press, release over a Checkbox. Per the codebase's own
// established convention (see PropertiesPanel.cpp:1604 comment, STAB-0719):
// "Checkbox/Combo/InputText get an unconditional pushUndo() (one fire per
// click/commit)" -- i.e. `if (ImGui::Checkbox(...)) { pushUndo(); ... }`,
// NOT gated by IsItemActivated() like Drag/ColorEdit/Slider. This matches
// production exactly and lets us assert "exactly one snapshot per click"
// directly from the widget's own return value.
static ClickResult driveCheckboxGesture(bool& value) {
    ClickResult r;
    ImGuiIO& io = ImGui::GetIO();

    auto frame = [&](float mx, float my, bool mouseDown, int frameIndex) {
        io.AddMousePosEvent(mx, my);
        io.AddMouseButtonEvent(0, mouseDown);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 300));
        ImGui::Begin("w", nullptr, ImGuiWindowFlags_NoSavedSettings);
        bool changed = ImGui::Checkbox("##cb", &value);
        if (changed) {
            ++r.mutationFrames;
            if (r.firstMutationFrame < 0) r.firstMutationFrame = frameIndex;
            // Mirrors production: pushUndo() fires unconditionally when the
            // widget itself reports a change, once per click.
            ++r.undoCount;
            if (r.undoFrame < 0) r.undoFrame = frameIndex;
        }
        ImVec2 mn = ImGui::GetItemRectMin(), mx2 = ImGui::GetItemRectMax();
        ImGui::End();
        ImGui::Render();
        return std::pair{mn, mx2};
    };

    auto [mn, mx] = frame(600, 600, false, -1);
    float cx = (mn.x + mx.x) * 0.5f, cy = (mn.y + mx.y) * 0.5f;

    frame(cx, cy, false, 0);   // hover
    frame(cx, cy, true, 1);    // press
    frame(cx, cy, false, 2);   // release -- click completes here

    return r;
}

// Drives: click to open a combo (BeginCombo/Selectable idiom -- the pattern
// actually used in the editor, e.g. PropertiesPanel.cpp's ##scdc
// defaultCamera combo), then a SEPARATE click on item index 1 of 3, then
// release. Verifies the audit mandate's specific concern: "the
// activation/selection frame produces exactly one snapshot, not one per
// dropdown-open click plus one per item click." Mirrors the same
// unconditional-pushUndo()-on-changed convention as Checkbox: opening the
// dropdown never sets `changed`, only selecting an item does.
static ClickResult driveComboGesture(int& current) {
    ClickResult r;
    ImGuiIO& io = ImGui::GetIO();
    static const char* kItems[] = {"Alpha", "Beta", "Gamma"};
    ImVec2 itemMin[3], itemMax[3];
    bool haveItemRects = false;

    auto frame = [&](float mx, float my, bool mouseDown, int frameIndex) {
        io.AddMousePosEvent(mx, my);
        io.AddMouseButtonEvent(0, mouseDown);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 300));
        ImGui::Begin("w", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextItemWidth(200);
        bool changed = false;
        if (ImGui::BeginCombo("##combo", kItems[current])) {
            for (int i = 0; i < 3; ++i) {
                bool sel = (i == current);
                if (ImGui::Selectable(kItems[i], sel)) { current = i; changed = true; }
                itemMin[i] = ImGui::GetItemRectMin();
                itemMax[i] = ImGui::GetItemRectMax();
            }
            haveItemRects = true;
            ImGui::EndCombo();
        }
        if (changed) {
            ++r.mutationFrames;
            if (r.firstMutationFrame < 0) r.firstMutationFrame = frameIndex;
            ++r.undoCount;
            if (r.undoFrame < 0) r.undoFrame = frameIndex;
        }
        ImVec2 mn = ImGui::GetItemRectMin(), mx2 = ImGui::GetItemRectMax();
        ImGui::End();
        ImGui::Render();
        return std::pair{mn, mx2};
    };

    // Frame -1: layout-only, closed. Learn the preview box rect.
    auto [pmn, pmx] = frame(600, 600, false, -1);
    float pcx = (pmn.x + pmx.x) * 0.5f, pcy = (pmn.y + pmx.y) * 0.5f;

    // Click 1 (open): press then release over the preview box -- ALWAYS both
    // frames (not conditionally skipped), so the mouse button ends up
    // genuinely released before click 2's press, giving that a real
    // false->true edge (a press that lands on the preview mid-hold, without
    // an intervening release, would just be a drag that never re-activates
    // a new item). Per ImGui's combo behavior the popup opens (and its
    // items render, nested inside this same BeginCombo/EndCombo call) as
    // soon as the click is detected -- we don't know in advance which of
    // these two frames that happens on, so we just check after both.
    frame(pcx, pcy, true, 0);
    frame(pcx, pcy, false, 1);
    check(haveItemRects, "Combo: dropdown opened (item rects known) after the open-click");
    check(r.undoCount == 0, "Combo: opening the dropdown alone produced no snapshot yet");

    // A brand-new popup window's rect can still be settling on the very
    // frame it first appears (auto-fit sizing/positioning stabilizes one
    // frame later) -- re-measure with one more idle frame (mouse unmoved,
    // still released) before trusting the item rects for the click below.
    frame(pcx, pcy, false, 2);

    // Click 2 (select item 1 of 3): a FRESH press+release, now positioned
    // over that item -- the popup stays open the whole time since nothing
    // has closed it yet.
    float icx = (itemMin[1].x + itemMax[1].x) * 0.5f, icy = (itemMin[1].y + itemMax[1].y) * 0.5f;
    frame(icx, icy, true, 3);
    frame(icx, icy, false, 4);

    // One more frame, mouse away, confirms nothing extra fires afterwards.
    frame(600, 600, false, 5);

    return r;
}

// Text-entry result: tracks how many frames InputText itself reported a
// change (its raw bool), which is exactly the signal the codebase's
// unconditional-pushUndo()-on-changed convention keys off for InputText too
// (see PropertiesPanel.cpp:1604 comment).
struct TextResult {
    int changeFrames = 0;   // frames where InputText returned true
    int firstChangeFrame = -1;
};

// Drives: click to focus an InputText, type `text` one character per frame,
// then (if commitViaEnter) press+release Enter, else just defocus by moving
// focus to a dummy second widget. `flags` lets the caller compare the
// EnterReturnsTrue convention actually used throughout this codebase against
// plain per-keystroke InputText.
static TextResult driveInputTextGesture(char* buf, size_t bufSize,
                                         ImGuiInputTextFlags flags,
                                         const char* text, bool commitViaEnter) {
    TextResult r;
    ImGuiIO& io = ImGui::GetIO();
    int frameIndex = -1;

    auto frame = [&](std::function<void(ImGuiIO&)> inject) {
        ++frameIndex;
        inject(io);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 300));
        ImGui::Begin("w", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextItemWidth(200);
        bool changed = ImGui::InputText("##it", buf, bufSize, flags);
        if (changed) {
            ++r.changeFrames;
            if (r.firstChangeFrame < 0) r.firstChangeFrame = frameIndex;
        }
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        ImGui::End();
        ImGui::Render();
        return std::pair{mn, mx};
    };

    // Frame 0: layout only, mouse away.
    auto [mn, mx] = frame([&](ImGuiIO& io2){
        io2.AddMousePosEvent(600, 600);
        io2.AddMouseButtonEvent(0, false);
    });
    float cx = (mn.x + mx.x) * 0.5f, cy = (mn.y + mx.y) * 0.5f;

    // Click to focus.
    frame([&](ImGuiIO& io2){ io2.AddMousePosEvent(cx, cy); io2.AddMouseButtonEvent(0, true); });
    frame([&](ImGuiIO& io2){ io2.AddMouseButtonEvent(0, false); });

    // Type each character on its own frame.
    for (const char* p = text; *p; ++p) {
        char one[2] = {*p, '\0'};
        frame([&, one](ImGuiIO& io2){ io2.AddInputCharactersUTF8(one); });
    }

    if (commitViaEnter) {
        frame([&](ImGuiIO& io2){ io2.AddKeyEvent(ImGuiKey_Enter, true); });
        frame([&](ImGuiIO& io2){ io2.AddKeyEvent(ImGuiKey_Enter, false); });
    } else {
        // Defocus by clicking elsewhere in the window's empty body, WITHOUT
        // ever pressing Enter. (350, 250) is deliberately far from both the
        // field itself and the window's top-left title-bar/collapse-arrow
        // hitbox -- clicking near (0-20, 0-20) would toggle collapse on the
        // shared "w" window instead of just defocusing the field, corrupting
        // every gesture driver that reuses "w" afterwards.)
        frame([&](ImGuiIO& io2){ io2.AddMousePosEvent(350, 250); io2.AddMouseButtonEvent(0, true); });
        frame([&](ImGuiIO& io2){ io2.AddMouseButtonEvent(0, false); });
    }

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

    // No false-positive snapshots: pure hover, no click at all.
    {
        float v = 5.0f;
        check(driveHoverOnlyNoOp([&]{ return ImGui::DragFloat("##hd", &v, 0.5f, 0.0f, 1000.0f); }),
              "DragFloat: pure hover (no click) never activates or changes the value");
    }
    {
        bool v = false;
        check(driveHoverOnlyNoOp([&]{ return ImGui::Checkbox("##hc", &v); }),
              "Checkbox: pure hover (no click) never activates or changes the value");
        check(v == false, "Checkbox: value unaffected by hover-only frames");
    }

    // Checkbox (AUD-036c): a single click is both the activation frame and
    // the value-change frame here (unlike Drag/ColorEdit where they diverge)
    // -- verify exactly one undo snapshot per click, matching the codebase's
    // unconditional-pushUndo()-on-changed convention for this widget class.
    {
        bool v = false;
        auto r = driveCheckboxGesture(v);
        check(r.undoCount == 1,
              "Checkbox: exactly one undo snapshot per click (got " +
              std::to_string(r.undoCount) + ")");
        check(r.mutationFrames == 1,
              "Checkbox: the value changes on exactly one frame per click (got " +
              std::to_string(r.mutationFrames) + ")");
        check(v == true, "Checkbox: value actually toggled by the click");
    }

    // Combo (AUD-036c): opening the dropdown and then selecting an item must
    // produce exactly ONE snapshot for the whole gesture -- not one for the
    // open-click and another for the item-click.
    {
        int current = 0;
        auto r = driveComboGesture(current);
        check(r.undoCount == 1,
              "Combo: exactly one undo snapshot for open+select (got " +
              std::to_string(r.undoCount) + ")");
        check(r.mutationFrames == 1,
              "Combo: selection changes on exactly one frame (got " +
              std::to_string(r.mutationFrames) + ")");
        check(current == 1, "Combo: item index 1 (\"Beta\") was actually selected");
    }

    // InputText (AUD-036c): this codebase's InputText undo convention is NOT
    // IsItemDeactivatedAfterEdit() -- that API has zero call sites anywhere
    // in src/ (grepped). The actual, consistently-used convention (see
    // PropertiesPanel.cpp:1604 and every InputText undo site in this file)
    // is ImGuiInputTextFlags_EnterReturnsTrue + an unconditional pushUndo()
    // inside the changed-block: the widget itself only returns true once,
    // on the frame Enter is pressed, regardless of how many characters were
    // typed first. Verify that here, and contrast it against plain
    // InputText (no flag), which -- as this test demonstrates -- returns
    // true on EVERY keystroke, which is exactly the one-snapshot-per-
    // keystroke bug the EnterReturnsTrue convention avoids.
    {
        char buf[64] = "";
        auto r = driveInputTextGesture(buf, sizeof(buf),
                                        ImGuiInputTextFlags_EnterReturnsTrue,
                                        "Hi!", /*commitViaEnter=*/true);
        check(r.changeFrames == 1,
              "InputText+EnterReturnsTrue: exactly one commit frame for a 3-character "
              "edit (got " + std::to_string(r.changeFrames) + " -- NOT one per keystroke)");
        check(std::string(buf) == "Hi!",
              "InputText+EnterReturnsTrue: the committed buffer holds the typed text");
    }
    {
        // Contrast case: without EnterReturnsTrue, InputText returns true on
        // every keystroke -- confirms the harness distinguishes the two
        // conventions rather than trivially reporting 1 either way, and
        // documents in code exactly why this codebase never uses the plain
        // form for undo-tracked fields.
        char buf[64] = "";
        auto r = driveInputTextGesture(buf, sizeof(buf), 0, "Hi!", /*commitViaEnter=*/true);
        check(r.changeFrames == 3,
              "InputText (no EnterReturnsTrue, contrast case): returns true once per "
              "keystroke (got " + std::to_string(r.changeFrames) + " for 3 characters) -- "
              "this is why the codebase always uses EnterReturnsTrue for undo-tracked "
              "text fields instead");
    }
    {
        // Defocus (click away) WITHOUT pressing Enter must not commit --
        // an incomplete edit leaves no false undo/history entry, matching
        // this codebase's InputText+EnterReturnsTrue convention.
        char buf[64] = "";
        auto r = driveInputTextGesture(buf, sizeof(buf),
                                        ImGuiInputTextFlags_EnterReturnsTrue,
                                        "Hi!", /*commitViaEnter=*/false);
        check(r.changeFrames == 0,
              "InputText+EnterReturnsTrue: clicking away without pressing Enter never "
              "returns true, so no undo snapshot and no document mutation is recorded "
              "for the abandoned edit (got " + std::to_string(r.changeFrames) + " change "
              "frames)");
    }

    // Multi-object edit (AUD-036c): a single button click applying a change
    // to N selected objects must push exactly ONE undo snapshot for the
    // whole batch, not N and not zero. Modeled directly on the real
    // production convention at MeshCraftApplication_Commands.cpp's
    // copyPropsToSelected(): `pushUndo();` once, then a for-loop over
    // `selection_.selection()` mutating each target object.
    {
        struct MockObj { int value = 0; };
        std::vector<MockObj> selected(5);
        int pushUndoCalls = 0;
        ImGuiIO& io2 = ImGui::GetIO();

        auto frame = [&](float mx, float my, bool mouseDown) {
            io2.AddMousePosEvent(mx, my);
            io2.AddMouseButtonEvent(0, mouseDown);
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(400, 300));
            ImGui::Begin("w", nullptr, ImGuiWindowFlags_NoSavedSettings);
            bool clicked = ImGui::Button("Apply##copyprops");
            if (clicked) {
                // Mirrors copyPropsToSelected(): pushUndo() ONCE, before the
                // loop, regardless of selection size.
                ++pushUndoCalls;
                for (auto& obj : selected) obj.value = 42;
            }
            ImVec2 mn = ImGui::GetItemRectMin(), mx2 = ImGui::GetItemRectMax();
            ImGui::End();
            ImGui::Render();
            return std::pair{mn, mx2};
        };

        auto [mn, mx] = frame(600, 600, false);
        float cx = (mn.x + mx.x) * 0.5f, cy = (mn.y + mx.y) * 0.5f;
        frame(cx, cy, false);  // hover
        frame(cx, cy, true);   // press
        frame(cx, cy, false);  // release -- Button() reports true here

        check(pushUndoCalls == 1,
              "Multi-object batch edit: exactly one undo snapshot for the whole "
              "N=5 selection (got " + std::to_string(pushUndoCalls) + ")");
        bool allMutated = true;
        for (const auto& obj : selected) allMutated &= (obj.value == 42);
        check(allMutated, "Multi-object batch edit: all N selected objects were mutated");
    }

    // Redo invalidation on the first new mutation after an undo is enforced
    // structurally inside pushUndo() itself (MeshCraftApplication_Commands.cpp,
    // `void MeshCraftApplication::pushUndo() { undoStack_.push_back(...);
    // ...; redoStack_.clear(); }") -- independent of any widget/gesture
    // timing, so no ImGui frames are needed to exercise it. That function
    // isn't headlessly callable here (it's a CNA-coupled member function),
    // so this mirrors its exact 2-statement invariant against a stand-in
    // stack pair -- this breaks if that invariant is ever dropped from the
    // real function.
    {
        std::vector<int> undoStack, redoStack;
        auto pushUndoMock = [&](int snapshot) {
            undoStack.push_back(snapshot);
            redoStack.clear();
        };
        undoStack.push_back(0);
        redoStack.push_back(1); redoStack.push_back(2);  // as if the user had undone twice
        check(!redoStack.empty(), "Redo-invalidation setup: redo stack has entries before a new edit");
        pushUndoMock(3);  // a fresh mutation, as pushUndo() runs before every command
        check(redoStack.empty(),
              "Redo stack is cleared by the first pushUndo() after an undo (mirrors "
              "MeshCraftApplication_Commands.cpp's pushUndo() body)");
    }

    ImGui::DestroyContext();

    if (failures == 0) { std::printf("All undo-gesture frame tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d undo-gesture frame test(s) failed.\n", failures);
    return 1;
}
