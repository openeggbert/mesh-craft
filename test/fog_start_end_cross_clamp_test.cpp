// F20 (2026-07-20 audit) — the two independent Environment/Fog editors
// (MeshCraftApplication_UiLeftPanel.cpp's "Env" tab, Scene/PropertiesPanel.cpp's
// "Scene Properties" section) had drifted: PropertiesPanel.cpp's Fog
// Start/End DragFloat fields were already cross-clamped (Start's max is
// End's current value, End's min is Start's current value) so they can
// never invert, but the Env tab clamped each independently to a fixed
// [0,10000] range, letting Start end up greater than End (a nonsensical
// fog range the renderer has no defined behavior for). The fix makes the
// Env tab use the exact same cross-clamped bounds PropertiesPanel.cpp
// already had.
//
// This drives REAL ImGui frames against the exact DragFloat call both
// editors now share (same widget, same ImGuiSliderFlags_AlwaysClamp, same
// dynamic min/max expressions) -- no CNA/MeshCraftApplication/
// PropertiesPanel dependency needed, since the property under test is
// ImGui's own clamping behavior against a plain Mc3Fog struct, not
// anything CNA-coupled. Mirrors undo_gesture_frame_test.cpp's established
// "drive real ImGui frames, no GL context needed" approach.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "MeshCraft/Mc3/Mc3Environment.hpp"

#include <cstdio>
#include <functional>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) std::printf("PASS: %s\n", msg.c_str());
    else      { std::printf("FAIL: %s\n", msg.c_str()); ++failures; }
}

// Drives a full click-drag gesture (hover, press, N move-while-held
// frames, release) over the given widget, exactly like
// undo_gesture_frame_test.cpp's driveClickDragGesture.
static void driveClickDragGesture(const std::function<void()>& drawWidget, float dragToX) {
    ImGuiIO& io = ImGui::GetIO();

    auto frame = [&](float mx, float my, bool mouseDown) {
        io.AddMousePosEvent(mx, my);
        io.AddMouseButtonEvent(0, mouseDown);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 300));
        ImGui::Begin("w", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextItemWidth(200);
        drawWidget();
        ImVec2 mn = ImGui::GetItemRectMin(), mx2 = ImGui::GetItemRectMax();
        ImGui::End();
        ImGui::Render();
        return std::pair{mn, mx2};
    };

    auto [mn, mx] = frame(600, 600, false);
    float cx = (mn.x + mx.x) * 0.5f, cy = (mn.y + mx.y) * 0.5f;

    frame(cx, cy, false);   // hover
    frame(cx, cy, true);    // press
    for (int i = 1; i <= 40; ++i)
        frame(dragToX, cy, true);
    frame(dragToX, cy, false);  // release
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

    // Fixed fog: Start=100, End=200. Dragging Start far to the right
    // (toward/past where End sits) must clamp Start at End's CURRENT
    // value (200), never exceed it -- both editors now use
    // `DragFloat("##fogstart", &fog.start, 0.5f, 0.0f, fog.end, ...)`.
    {
        Mc3Fog fog;
        fog.start = 100.0f;
        fog.end   = 200.0f;

        driveClickDragGesture([&] {
            ImGui::DragFloat("##fogstart", &fog.start, 0.5f, 0.0f, fog.end,
                              "%.3f", ImGuiSliderFlags_AlwaysClamp);
        }, /*dragToX=*/5000.0f);  // drag far right -- would exceed End with no clamp

        check(fog.start <= fog.end,
              "F20: dragging Start far past End clamps at End's value, never inverts "
              "(start=" + std::to_string(fog.start) + ", end=" + std::to_string(fog.end) + ")");
        check(fog.start <= 200.0f + 0.001f,
              "F20: Start's clamped value does not exceed End's current value (200)");
    }

    // Symmetric case: dragging End far to the LEFT (toward/past Start)
    // must clamp End at Start's current value, never go below it --
    // `DragFloat("##fogend", &fog.end, 0.5f, fog.start, 10000.0f, ...)`.
    {
        Mc3Fog fog;
        fog.start = 100.0f;
        fog.end   = 200.0f;

        driveClickDragGesture([&] {
            ImGui::DragFloat("##fogend", &fog.end, 0.5f, fog.start, 10000.0f,
                              "%.3f", ImGuiSliderFlags_AlwaysClamp);
        }, /*dragToX=*/-5000.0f);  // drag far left -- would go below Start with no clamp

        check(fog.end >= fog.start,
              "F20: dragging End far past Start clamps at Start's value, never inverts "
              "(start=" + std::to_string(fog.start) + ", end=" + std::to_string(fog.end) + ")");
        check(fog.end >= 100.0f - 0.001f,
              "F20: End's clamped value does not go below Start's current value (100)");
    }

    ImGui::DestroyContext();

    if (failures == 0) { std::printf("All fog start/end cross-clamp tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d fog start/end cross-clamp test(s) failed.\n", failures);
    return 1;
}
