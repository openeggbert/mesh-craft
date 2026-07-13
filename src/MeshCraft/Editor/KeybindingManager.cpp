#include "MeshCraft/Editor/KeybindingManager.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <vector>

namespace MeshCraft::Editor {

using namespace Microsoft::Xna::Framework::Input;

// ---------------------------------------------------------------------------
// Key name <-> Keys:: enum conversion table
// ---------------------------------------------------------------------------
namespace {

struct KeyEntry { int key; const char* name; };
const KeyEntry kKeyTable[] = {
    { (int)Keys::A,"A" }, { (int)Keys::B,"B" }, { (int)Keys::C,"C" },
    { (int)Keys::D,"D" }, { (int)Keys::E,"E" }, { (int)Keys::F,"F" },
    { (int)Keys::G,"G" }, { (int)Keys::H,"H" }, { (int)Keys::I,"I" },
    { (int)Keys::J,"J" }, { (int)Keys::K,"K" }, { (int)Keys::L,"L" },
    { (int)Keys::M,"M" }, { (int)Keys::N,"N" }, { (int)Keys::O,"O" },
    { (int)Keys::P,"P" }, { (int)Keys::Q,"Q" }, { (int)Keys::R,"R" },
    { (int)Keys::S,"S" }, { (int)Keys::T,"T" }, { (int)Keys::U,"U" },
    { (int)Keys::V,"V" }, { (int)Keys::W,"W" }, { (int)Keys::X,"X" },
    { (int)Keys::Y,"Y" }, { (int)Keys::Z,"Z" },
    { (int)Keys::F1,"F1" },   { (int)Keys::F2,"F2" },  { (int)Keys::F3,"F3" },
    { (int)Keys::F4,"F4" },   { (int)Keys::F5,"F5" },  { (int)Keys::F6,"F6" },
    { (int)Keys::F7,"F7" },   { (int)Keys::F8,"F8" },  { (int)Keys::F9,"F9" },
    { (int)Keys::F10,"F10" }, { (int)Keys::F11,"F11" },{ (int)Keys::F12,"F12" },
    { (int)Keys::NumPad1,"Num1" }, { (int)Keys::NumPad2,"Num2" },
    { (int)Keys::NumPad3,"Num3" }, { (int)Keys::NumPad4,"Num4" },
    { (int)Keys::NumPad5,"Num5" }, { (int)Keys::NumPad6,"Num6" },
    { (int)Keys::NumPad7,"Num7" }, { (int)Keys::NumPad8,"Num8" },
    { (int)Keys::NumPad9,"Num9" }, { (int)Keys::NumPad0,"Num0" },
    { (int)Keys::Space,   "Space"   }, { (int)Keys::Delete,  "Delete" },
    { (int)Keys::Escape,  "Escape"  }, { (int)Keys::Tab,     "Tab"    },
    { (int)Keys::Up,      "Up"      }, { (int)Keys::Down,    "Down"   },
    { (int)Keys::Left,    "Left"    }, { (int)Keys::Right,   "Right"  },
    { (int)Keys::PageUp,  "PageUp"  }, { (int)Keys::PageDown,"PageDown"},
    { (int)Keys::Home,    "Home"    }, { (int)Keys::End,     "End"    },
    { (int)Keys::Insert,  "Insert"  },
};

const char* keyToName(int key) {
    for (auto& e : kKeyTable) if (e.key == key) return e.name;
    return "?";
}

int nameToKey(const std::string& n) {
    std::string upper = n;
    for (auto& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (auto& e : kKeyTable) {
        std::string en = e.name;
        for (auto& c : en) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (en == upper) return e.key;
    }
    return 0;
}

// Local edge-detect predicate -- deliberately not sharing
// MeshCraftPrivate.hpp's justPressed(): that header also pulls in
// Mc3Document/ObjectTypeName, which this class has no other reason to
// depend on (KeybindingManager knows nothing about the scene document).
bool justPressed(const KeyboardState& cur, const KeyboardState& prev, Keys k) {
    return cur.IsKeyDown(k) && prev.IsKeyUp(k);
}

} // namespace

// ---------------------------------------------------------------------------
// KeyBind
// ---------------------------------------------------------------------------

std::string KeyBind::toLabel() const {
    if (!key) return "(unbound)";
    std::string s;
    if (ctrl)  s += "Ctrl+";
    if (shift) s += "Shift+";
    if (alt)   s += "Alt+";
    s += keyToName(key);
    return s;
}

std::string KeyBind::toString() const {
    if (!key) return "";
    std::string s;
    if (ctrl)  s += "ctrl+";
    if (shift) s += "shift+";
    if (alt)   s += "alt+";
    s += keyToName(key);
    return s;
}

KeyBind KeyBind::fromString(const std::string& raw) {
    KeyBind b;
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
        else                    b.key   = nameToKey(p);
    }
    return b;
}

// ---------------------------------------------------------------------------
// KeybindingManager
// ---------------------------------------------------------------------------

void KeybindingManager::initDefaults() {
    auto add = [&](const char* id, bool c, bool s, bool a, int k) {
        if (!bindings_.count(id))
            bindings_[id] = KeyBind{c, s, a, k};
    };
    using K = Keys;
    add("file.new",         true,  false, false, (int)K::N);
    add("file.open",        true,  false, false, (int)K::O);
    add("file.save",        true,  false, false, (int)K::S);
    add("file.saveAs",      true,  true,  false, (int)K::S);
    add("file.export",      true,  false, false, (int)K::E);
    add("edit.undo",        true,  false, false, (int)K::Z);
    add("edit.redo",        true,  false, false, (int)K::Y);
    add("edit.cut",         true,  false, false, (int)K::X);
    add("edit.copy",        true,  false, false, (int)K::C);
    add("edit.paste",       true,  false, false, (int)K::V);
    add("edit.duplicate",   true,  false, false, (int)K::D);
    add("edit.delete",      false, false, false, (int)K::Delete);
    add("edit.selectAll",   true,  false, false, (int)K::A);
    add("edit.invertSel",   true,  false, false, (int)K::I);
    add("edit.group",       true,  false, false, (int)K::G);
    add("edit.ungroup",     true,  true,  false, (int)K::G);
    add("edit.batchRename", true,  true,  false, (int)K::R);
    add("edit.findReplace", true,  false, false, (int)K::H);
    add("edit.lock",        true,  false, false, (int)K::L);
    add("view.timeline",    true,  false, false, (int)K::T);
    add("view.hideSelected",false, false, false, (int)K::H);
    add("view.showAll",     false, false, true,  (int)K::H);
    add("view.isolate",     false, false, true,  (int)K::I);
    add("view.edgeOverlay", false, false, true,  (int)K::W);
    add("view.focus",       false, false, false, (int)K::F);
    add("tool.select",      false, false, false, (int)K::Q);
    add("tool.move",        false, false, false, (int)K::G);
    add("tool.scale",       false, false, false, (int)K::S);
    add("tool.rotate",      false, false, false, (int)K::R);
    add("anim.playPause",   false, false, false, (int)K::Space);
    add("ui.cmdPalette",    true,  false, false, (int)K::P);
    add("ui.screenshot",    false, false, false, (int)K::F11);
}

void KeybindingManager::load(const std::filesystem::path& path) {
    initDefaults();
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string id  = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        bindings_[id] = KeyBind::fromString(val);
    }
}

void KeybindingManager::save(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path);
    if (!f) return;
    for (const auto& [id, bind] : bindings_)
        f << id << "=" << bind.toString() << "\n";
}

bool KeybindingManager::shortcutFired(const std::string& id,
                                       const KeyboardState& ks,
                                       const KeyboardState& prev) const {
    auto it = bindings_.find(id);
    if (it == bindings_.end() || !it->second.key) return false;
    const KeyBind& b = it->second;
    bool ctrl  = ks.IsKeyDown(Keys::LeftControl) || ks.IsKeyDown(Keys::RightControl);
    bool shift = ks.IsKeyDown(Keys::LeftShift)   || ks.IsKeyDown(Keys::RightShift);
    bool alt   = ks.IsKeyDown(Keys::LeftAlt)     || ks.IsKeyDown(Keys::RightAlt);
    if (b.ctrl != ctrl || b.shift != shift || b.alt != alt) return false;
    return justPressed(ks, prev, static_cast<Keys>(b.key));
}

} // namespace MeshCraft::Editor
