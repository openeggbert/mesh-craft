#pragma once
// Pure editor PREFERENCES/persistence-format algorithms (keybinding
// serialization, preferences ini, macro save/load) — no CNA / ImGui / SDL /
// OpenGL dependencies, and no Mc3 document dependency either: every
// function here operates on its own small value types (KeyBindAlg,
// PrefsAlg, MacroStepAlg), not Mc3::Mc3Document/Mc3Object.
//
// SYS-W3-05: split out of the former monolithic EditorAlgorithms.hpp so
// consumers that only need these small persistence formats don't have to
// pull in selection, command-mutation, transform-drag, persistence
// (document-level), event, and utility algorithms they never call.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace MeshCraft {

// ── Keybinding persistence format (STAB-0286) ─────────────────────────────────
//
// Mirrors the persistence format used by Editor::KeyBind::toString()/
// fromString() and Editor::KeybindingManager::load()/save() (SYS-W3-01:
// relocated from MeshCraftApplication_Keybindings.cpp into
// MeshCraft/Editor/KeybindingManager.{hpp,cpp}): each binding serializes as
// "id=[ctrl+][shift+][alt+]KEYNAME" (or an empty value when unbound), one
// "id=value" line per binding; loading re-parses the same tokens
// case-insensitively and only overwrites ids present in the file (an id
// absent from the file keeps its pre-load — i.e. default — value, the same
// merge behavior `KeybindingManager::initDefaults()` + `load()` produce
// together in the real code). Uses its own small key-name table — distinct
// from the real `Keys::` enum, which lives in CNA and can't be included here
// — because what's under test is the tokenize/join *format*, not any
// particular `Keys::` integer value.

struct KeyBindAlg {
    bool ctrl{false}, shift{false}, alt{false};
    int  key{0};
};

namespace detail {
inline const std::vector<std::pair<int, const char*>>& keyNameTableAlg()
{
    static const std::vector<std::pair<int, const char*>> table = {
        {1, "A"}, {2, "S"}, {3, "Z"}, {4, "F1"}, {5, "Space"}, {6, "Delete"},
    };
    return table;
}
} // namespace detail

inline const char* keyNameAlg(int key)
{
    for (auto& [k, n] : detail::keyNameTableAlg()) if (k == key) return n;
    return "?";
}

inline int keyFromNameAlg(const std::string& n)
{
    std::string upper = n;
    for (auto& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (auto& [k, name] : detail::keyNameTableAlg()) {
        std::string en = name;
        for (auto& c : en) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (en == upper) return k;
    }
    return 0;
}

inline std::string keyBindToStringAlg(const KeyBindAlg& b)
{
    if (!b.key) return "";
    std::string s;
    if (b.ctrl)  s += "ctrl+";
    if (b.shift) s += "shift+";
    if (b.alt)   s += "alt+";
    s += keyNameAlg(b.key);
    return s;
}

inline KeyBindAlg keyBindFromStringAlg(const std::string& raw)
{
    KeyBindAlg b;
    if (raw.empty()) return b;
    std::vector<std::string> parts;
    std::string cur;
    for (char c : raw) {
        if (c == '+') { if (!cur.empty()) { parts.push_back(cur); cur.clear(); } }
        else cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    for (auto& p : parts) {
        std::string pl = p;
        for (auto& c : pl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (pl == "ctrl")  b.ctrl  = true;
        else if (pl == "shift") b.shift = true;
        else if (pl == "alt")   b.alt   = true;
        else                    b.key   = keyFromNameAlg(p);
    }
    return b;
}

inline void saveKeybindingsAlg(const std::filesystem::path& path,
                               const std::map<std::string, KeyBindAlg>& bindings)
{
    std::ofstream f(path);
    for (const auto& [id, bind] : bindings)
        f << id << "=" << keyBindToStringAlg(bind) << "\n";
}

inline void loadKeybindingsAlg(const std::filesystem::path& path,
                               std::map<std::string, KeyBindAlg>& bindings)
{
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string id  = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        bindings[id] = keyBindFromStringAlg(val);
    }
}

// ── Preferences persistence (STAB-0287) ───────────────────────────────────────
//
// Mirrors loadPrefs()/savePrefs() (MeshCraftApplication_FileOps.cpp:296-329):
// a flat "key=value" ini file for the six scalar preference fields. An
// unknown key or an unparseable value is silently skipped — one bad line
// must not prevent the rest of the file from loading — and a missing file
// leaves every field at its pre-load (caller-supplied default) value.
// applyTheme()'s ImGui side effect is intentionally not mirrored (rendering
// only, not data).

// AUD-031: snapTranslate/snapScale defaults here had drifted from
// MeshCraftApplication's real member-initializer defaults (1.0f/0.1f here
// vs the real 0.5f/0.25f) -- exactly the kind of silent divergence this
// finding warns two hand-synced copies are prone to. Corrected to match.
struct PrefsAlg {
    float autoSaveInterval{60.0f};
    float snapTranslate{0.5f};
    float snapRotate{15.0f};
    float snapScale{0.25f};
    float gridSpacing{1.0f};
    int   theme{0};
};

inline void savePrefsAlg(const std::filesystem::path& path, const PrefsAlg& p)
{
    std::ofstream f(path);
    if (!f) return;
    f << "autoSaveInterval=" << p.autoSaveInterval << "\n";
    f << "snapTranslate="    << p.snapTranslate    << "\n";
    f << "snapRotate="       << p.snapRotate       << "\n";
    f << "snapScale="        << p.snapScale        << "\n";
    f << "gridSpacing="      << p.gridSpacing      << "\n";
    f << "theme="            << p.theme            << "\n";
}

inline void loadPrefsAlg(const std::filesystem::path& path, PrefsAlg& p)
{
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        try {
            // AUD-032: clamped to the same bounds production loadPrefs()
            // (MeshCraftApplication_FileOps.cpp) uses -- the widest range
            // any slider UI for this value allows -- so a hand-edited
            // prefs.ini can't set a value neither slider could ever reach,
            // e.g. 0 or negative snapScale. Values here must be kept in
            // sync with loadPrefs() by hand; there is no shared constant.
            if      (key == "autoSaveInterval") p.autoSaveInterval = std::clamp(std::stof(val), 0.0f, 300.0f);
            else if (key == "snapTranslate")    p.snapTranslate    = std::clamp(std::stof(val), 0.01f, 100.0f);
            else if (key == "snapRotate")       p.snapRotate       = std::clamp(std::stof(val), 1.0f, 180.0f);
            else if (key == "snapScale")        p.snapScale        = std::clamp(std::stof(val), 0.01f, 10.0f);
            else if (key == "gridSpacing")      p.gridSpacing      = std::clamp(std::stof(val), 0.1f, 10.0f);
            else if (key == "theme")            p.theme            = std::clamp(std::stoi(val), 0, 2);
        } catch (...) {}
    }
}

// ── Macro save/load (STAB-0292) ───────────────────────────────────────────────
//
// Mirrors Editor::MacroRecorder::save()/load() (SYS-W3-01 Phase 3,
// src/MeshCraft/Editor/MacroRecorder.cpp): each step is one tab-separated
// line ("verb\targ1\targ2..."); loading skips blank lines. MacroStepAlg
// mirrors Editor::MacroRecorder::Step's fields exactly -- kept as a
// separate type since these Alg functions are unit-tested standalone
// without linking MacroRecorder itself.

struct MacroStepAlg {
    std::string              verb;
    std::vector<std::string> args;
};

inline void saveMacroAlg(const std::filesystem::path& path,
                         const std::vector<MacroStepAlg>& steps)
{
    std::ofstream f(path);
    for (const auto& step : steps) {
        f << step.verb;
        for (const auto& arg : step.args) f << '\t' << arg;
        f << '\n';
    }
}

inline std::vector<MacroStepAlg> loadMacroAlg(const std::filesystem::path& path)
{
    std::vector<MacroStepAlg> steps;
    std::ifstream f(path);
    if (!f) return steps;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        MacroStepAlg step;
        std::istringstream ss(line);
        std::string tok;
        bool first = true;
        while (std::getline(ss, tok, '\t')) {
            if (first) { step.verb = tok; first = false; }
            else step.args.push_back(tok);
        }
        if (!step.verb.empty()) steps.push_back(std::move(step));
    }
    return steps;
}

} // namespace MeshCraft
