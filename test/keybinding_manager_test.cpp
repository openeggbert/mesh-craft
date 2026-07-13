// Editor::KeybindingManager test (SYS-W3-01 Phase 1).
//
// No test existed for the keybinding subsystem before this extraction --
// the previous MeshCraftApplication_Keybindings.cpp had zero coverage.
// Covers default-seeding, shortcutFired()'s modifier-matching + just-pressed
// edge detection, the save/load round-trip (including that a user rebind
// survives while untouched ids still re-seed to their default), and
// KeyBind::toString()/fromString().

#include "MeshCraft/Editor/KeybindingManager.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>

#include <cstdio>
#include <filesystem>

using namespace MeshCraft::Editor;
using namespace Microsoft::Xna::Framework::Input;

static int failures = 0;
static void check(bool cond, const char* msg) {
    if (cond) std::printf("PASS: %s\n", msg);
    else      { std::printf("FAIL: %s\n", msg); ++failures; }
}

int main() {
    KeybindingManager kb;
    kb.initDefaults();
    check(kb.bindings().count("file.save") == 1, "file.save default exists");
    check(kb.bindings()["file.save"].ctrl && kb.bindings()["file.save"].key == (int)Keys::S,
          "file.save defaults to Ctrl+S");
    check(kb.bindings()["file.save"].toLabel() == "Ctrl+S", "toLabel() renders 'Ctrl+S'");

    KeyboardState prevNone{};
    KeyboardState curCtrlS{Keys::LeftControl, Keys::S};
    check(kb.shortcutFired("file.save", curCtrlS, prevNone),
          "shortcutFired fires for Ctrl+S just-pressed");
    KeyboardState curSOnly{Keys::S};
    check(!kb.shortcutFired("file.save", curSOnly, prevNone),
          "shortcutFired does NOT fire for S alone (missing Ctrl)");
    check(!kb.shortcutFired("file.save", curCtrlS, curCtrlS),
          "shortcutFired does NOT fire when the key was already down last frame (held, not just-pressed)");
    check(!kb.shortcutFired("no.such.action", curCtrlS, prevNone),
          "shortcutFired returns false for an unknown action id");

    auto path = std::filesystem::temp_directory_path() / "meshcraft_keybinding_manager_test.ini";
    std::filesystem::remove(path);
    kb.bindings()["file.save"] = KeyBind{true, true, false, (int)Keys::S}; // rebind to Ctrl+Shift+S
    kb.save(path);
    check(std::filesystem::exists(path), "save() writes the ini file");

    KeybindingManager kb2;
    kb2.load(path);
    check(kb2.bindings()["file.save"].shift && kb2.bindings()["file.save"].ctrl,
          "load() restores the user's rebind (Ctrl+Shift+S) exactly");
    check(kb2.bindings().count("file.open") == 1 && kb2.bindings()["file.open"].key == (int)Keys::O,
          "load() also seeds defaults for ids not present in the file (file.open untouched)");

    KeyBind parsed = KeyBind::fromString("ctrl+shift+alt+f5");
    check(parsed.ctrl && parsed.shift && parsed.alt && parsed.key == (int)Keys::F5,
          "KeyBind::fromString parses all three modifiers plus a function key");
    check(parsed.toString() == "ctrl+shift+alt+F5", "KeyBind::toString round-trips fromString's output");

    std::error_code ec;
    std::filesystem::remove(path, ec);

    if (failures == 0) { std::printf("\nAll keybinding_manager checks passed.\n"); return 0; }
    std::printf("\n%d check(s) FAILED.\n", failures);
    return 1;
}
