#pragma once

#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace MeshCraft::Editor {

// H9: a single customizable key binding.
struct KeyBind {
    bool ctrl{false}, shift{false}, alt{false};
    int  key{0};                 // Keys:: enum value; 0 = unbound
    std::string toLabel() const; // e.g. "Ctrl+S"
    static KeyBind fromString(const std::string& s);
    std::string toString() const;
};

// SYS-W3-01: the customizable-keybindings subsystem, extracted out of
// MeshCraftApplication (H9). Self-contained: no Mc3Document/app-state
// dependency, matching the SelectionManager/EditorCamera/AiAssistant idiom
// already established in this codebase (value member, zero-arg
// constructor, whatever it needs to know is either owned here or passed
// per-call).
class KeybindingManager {
public:
    KeybindingManager() = default;

    // Seeds every action id with its documented default, without
    // overwriting an id that's already bound (safe to call more than once).
    void initDefaults();

    // initDefaults(), then overlays any overrides found in `path`'s
    // "id=ctrl+shift+alt+key" lines. Missing file / unreadable id lines are
    // silently skipped, matching the original loadKeybindings() behavior.
    void load(const std::filesystem::path& path);

    // Writes every current binding to `path` as "id=<KeyBind::toString()>"
    // lines, one per line. No-ops (does not throw) if the file can't be
    // opened for writing.
    void save(const std::filesystem::path& path) const;

    // True if `id`'s bound modifiers match ks's current modifier state AND
    // its key was just pressed this frame (down in `ks`, not down in `prev`).
    // False for an unbound (key == 0) or unknown id.
    [[nodiscard]] bool shortcutFired(
        const std::string& id,
        const Microsoft::Xna::Framework::Input::KeyboardState& ks,
        const Microsoft::Xna::Framework::Input::KeyboardState& prev) const;

    [[nodiscard]] std::unordered_map<std::string, KeyBind>& bindings() { return bindings_; }
    [[nodiscard]] const std::unordered_map<std::string, KeyBind>& bindings() const {
        return bindings_;
    }

private:
    std::unordered_map<std::string, KeyBind> bindings_;
};

} // namespace MeshCraft::Editor
