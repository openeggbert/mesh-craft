#pragma once

namespace MeshCraft::Editor {

// SYS-W3-01 Phase 2 (human-authorized decision, 2026-07-17: narrow scope):
// the theme-preference subsystem extracted out of MeshCraftApplication
// (H7). Deliberately narrow -- autoSaveInterval_/snapTranslate_/
// snapRotate_/snapScale_/gridSpacing_ stay on MeshCraftApplication for now
// (their own future extraction) since MeshCraftApplication::loadPrefs()/
// savePrefs() persist all of those together with the theme in one prefs
// file via PrefsAlg, and splitting that shared load/save mechanism is out
// of scope for this narrow extraction. This class owns only the runtime
// theme state and the ImGui style-apply behavior, following the
// KeybindingManager/SelectionManager extraction idiom: self-contained
// value member, zero-arg constructor, whatever it needs from elsewhere is
// passed per-call rather than stored.
class Preferences {
public:
    Preferences() = default;

    // 0=Dark, 1=Light, 2=Classic.
    [[nodiscard]] int  theme() const { return theme_; }
    void setTheme(int theme) { theme_ = theme; }

    // Whether the Preferences dialog is currently requested/open. Pure UI
    // state, never persisted to disk.
    [[nodiscard]] bool windowOpen() const { return windowOpen_; }
    void setWindowOpen(bool open) { windowOpen_ = open; }

    // Applies theme() as the current ImGui style.
    void applyTheme() const;

private:
    int  theme_{0};
    bool windowOpen_{false};
};

} // namespace MeshCraft::Editor
