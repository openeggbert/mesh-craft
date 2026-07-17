// Editor::Preferences test (SYS-W3-01 Phase 2, narrow scope).
//
// No test existed for the theme-preference subsystem before this
// extraction. Covers the default theme, the theme()/setTheme() and
// windowOpen()/setWindowOpen() accessors, and that applyTheme() actually
// dispatches to a different ImGui style per theme value (not just that it
// doesn't crash).

#include "MeshCraft/Editor/Preferences.hpp"

#include <imgui.h>

#include <cstdio>

using namespace MeshCraft::Editor;

static int failures = 0;
static void check(bool cond, const char* msg) {
    if (cond) std::printf("PASS: %s\n", msg);
    else      { std::printf("FAIL: %s\n", msg); ++failures; }
}

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    Preferences prefs;
    check(prefs.theme() == 0, "default theme is 0 (Dark)");
    check(!prefs.windowOpen(), "default windowOpen is false");

    prefs.setTheme(2);
    check(prefs.theme() == 2, "setTheme(2) survives in theme()");

    prefs.setWindowOpen(true);
    check(prefs.windowOpen(), "setWindowOpen(true) survives in windowOpen()");
    prefs.setWindowOpen(false);
    check(!prefs.windowOpen(), "setWindowOpen(false) survives in windowOpen()");

    // Dark vs. Light have a well-known, version-stable distinction: Dark's
    // WindowBg is near-black, Light's is near-white. Use that as evidence
    // applyTheme() actually dispatched by theme(), not just that it ran.
    prefs.setTheme(0);
    prefs.applyTheme();
    float darkBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x;
    check(darkBg < 0.5f, "theme 0 (Dark) applies a dark WindowBg");

    prefs.setTheme(1);
    prefs.applyTheme();
    float lightBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x;
    check(lightBg > 0.5f, "theme 1 (Light) applies a light WindowBg");
    check(lightBg != darkBg, "Dark and Light apply visibly different WindowBg colors");

    prefs.setTheme(2);
    prefs.applyTheme();
    float classicBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x;
    check(classicBg != lightBg, "theme 2 (Classic) differs from Light");

    // Out-of-range theme values fall through to the `default:` (Dark)
    // branch rather than indexing out of bounds or crashing.
    prefs.setTheme(99);
    prefs.applyTheme();
    float fallbackBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x;
    check(fallbackBg == darkBg, "an out-of-range theme value falls back to Dark, not a crash");

    ImGui::DestroyContext();

    if (failures == 0) { std::printf("All preferences tests passed.\n"); return 0; }
    std::printf("%d preferences test(s) failed.\n", failures);
    return 1;
}
