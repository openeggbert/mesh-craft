#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"

#include <SDL3/SDL.h>

#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/Viewport.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <string>

namespace MeshCraft {

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Graphics;

void MeshCraftApplication::addPrimitive(Mc3::ObjectType type) {
    pushUndo();
    auto obj = std::make_shared<Mc3::Mc3Object>();
    obj->type = type;

    static int counter = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Object%d", ++counter);
    obj->name = buf;
    obj->id   = buf;

    switch (type) {
    case Mc3::ObjectType::Box:      { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Box;      p.size = {1.0f,1.0f,1.0f}; obj->primitive = p; } break;
    case Mc3::ObjectType::Sphere:   { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Sphere;   p.radius = 0.5f;            obj->primitive = p; } break;
    case Mc3::ObjectType::Cylinder: { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Cylinder; p.radius = 0.5f; p.height = 1.0f; obj->primitive = p; } break;
    case Mc3::ObjectType::Cone:     { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Cone;     p.radius = 0.5f; p.height = 1.0f; obj->primitive = p; } break;
    case Mc3::ObjectType::Plane:    { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Plane;    p.size = {1.0f,0.0f,1.0f};  obj->primitive = p; } break;
    case Mc3::ObjectType::Torus:    { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Torus; p.majorRadius = 0.35f; p.minorRadius = 0.15f; p.segments = 32; obj->primitive = p; } break;
    case Mc3::ObjectType::Extrude: {
        Mc3::Mc3Extrude ex;
        ex.crossSection.type   = Mc3::CrossSectionType::Rect;
        ex.crossSection.width  = 0.3f;
        ex.crossSection.height = 0.3f;
        ex.path.type   = Mc3::ExtrudePathType::Line;
        ex.path.length = 1.0f;
        ex.path.axis   = "y";
        ex.segments    = 8;
        ex.smooth      = true;
        ex.caps        = true;
        obj->extrude = ex;
        break;
    }
    case Mc3::ObjectType::Union: {
        Mc3::Mc3CsgOperation csg; csg.csgType = Mc3::CsgType::Union;
        obj->csgOperation = csg; break;
    }
    case Mc3::ObjectType::Difference: {
        Mc3::Mc3CsgOperation csg; csg.csgType = Mc3::CsgType::Difference;
        obj->csgOperation = csg; break;
    }
    case Mc3::ObjectType::Intersection: {
        Mc3::Mc3CsgOperation csg; csg.csgType = Mc3::CsgType::Intersection;
        obj->csgOperation = csg; break;
    }
    default: break; // Group, Area, Mesh, Instance — no primitive
    }
    obj->transform.position = { camera_.target.X, camera_.target.Y + 0.5f, camera_.target.Z };

    if (selection_.hasSelection()) {
        auto& sel0 = selection_.selection().front();
        bool isGroup = sel0->type == Mc3::ObjectType::Group   ||
                       sel0->type == Mc3::ObjectType::Union   ||
                       sel0->type == Mc3::ObjectType::Difference ||
                       sel0->type == Mc3::ObjectType::Intersection ||
                       !sel0->children.empty();
        if (isGroup) {
            sel0->children.push_back(obj);
            selection_.clear(); selection_.select(obj);
            modified_ = true;
            updateWindowTitle(); return;
        }
    }
    document_.objects.push_back(obj);
    selection_.clear(); selection_.select(obj);
    modified_ = true;
    updateWindowTitle();
}

// (removeFromList is defined in MeshCraftPrivate.hpp)

void MeshCraftApplication::deleteSelected() {
    pushUndo();
    for (const auto& s : selection_.selection()) {
        if (lockedIds_.count(s->id)) continue;
        removeFromList(document_.objects, s.get());
    }
    selection_.clear();
    modified_ = true;
    updateWindowTitle();
}
void MeshCraftApplication::toggleIsolate() {
    std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> walk;
    if (!isolateActive_) {
        // Activate: save visibility, hide non-selected
        preisolateVisibility_.clear();
        walk = [&](auto& list) {
            for (auto& o : list) {
                preisolateVisibility_[o->id] = o->visible;
                if (!selection_.isSelected(o.get())) o->visible = false;
                walk(o->children);
            }
        };
        walk(document_.objects);
        isolateActive_ = true;
        setStatusMsg("Isolation ON — Alt+I to exit", false, 2.5f);
    } else {
        // Deactivate: restore saved visibility
        walk = [&](auto& list) {
            for (auto& o : list) {
                auto it = preisolateVisibility_.find(o->id);
                if (it != preisolateVisibility_.end()) o->visible = it->second;
                walk(o->children);
            }
        };
        walk(document_.objects);
        preisolateVisibility_.clear();
        isolateActive_ = false;
        setStatusMsg("Isolation OFF", false, 1.5f);
    }
    modified_ = true;
    updateWindowTitle();
}

void MeshCraftApplication::saveCameraBookmark(int slot) {
    auto& bm     = cameraBookmarks_[slot];
    bm.yaw       = camera_.yaw;
    bm.pitch     = camera_.pitch;
    bm.distance  = camera_.distance;
    bm.targetX   = camera_.target.X;
    bm.targetY   = camera_.target.Y;
    bm.targetZ   = camera_.target.Z;
    bm.valid     = true;
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Camera saved to slot %d", slot + 1);
    setStatusMsg(msg, false, 2.0f);
}

void MeshCraftApplication::restoreCameraBookmark(int slot) {
    const auto& bm = cameraBookmarks_[slot];
    if (!bm.valid) { setStatusMsg("Slot is empty", true, 1.5f); return; }
    camera_.yaw      = bm.yaw;
    camera_.pitch    = bm.pitch;
    camera_.distance = bm.distance;
    camera_.target   = { bm.targetX, bm.targetY, bm.targetZ };
}

// (deepCopyObject and findParentList are defined in MeshCraftPrivate.hpp)

void MeshCraftApplication::duplicateSelected() {
    if (!selection_.hasSelection()) return;
    pushUndo();
    auto prev = selection_.selection();
    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;

    for (const auto& s : prev) {
        auto* parent = findParentList(document_.objects, s.get());
        if (!parent) continue;
        auto copy = deepCopyObject(*s);
        copy->name = s->name + "_copy";
        copy->id = s->id.empty() ? copy->name : s->id + "_copy";
        auto it = std::find_if(parent->begin(), parent->end(),
            [&](const auto& o){ return o.get() == s.get(); });
        if (it != parent->end()) ++it;
        parent->insert(it, copy);
        newObjs.push_back(copy);
    }

    if (!newObjs.empty()) {
        selection_.clear();
        for (auto& o : newObjs) selection_.select(o);
        modified_ = true;
        updateWindowTitle();
    }
}

void MeshCraftApplication::copySelected() {
    if (!selection_.hasSelection()) return;
    clipboard_.clear();
    for (const auto& s : selection_.selection())
        clipboard_.push_back(deepCopyObject(*s));
}

void MeshCraftApplication::cutSelected() {
    if (!selection_.hasSelection()) return;
    copySelected();
    deleteSelected();
}

void MeshCraftApplication::pasteClipboard() {
    if (clipboard_.empty()) return;
    pushUndo();
    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;
    for (const auto& src : clipboard_) {
        auto copy = deepCopyObject(*src);
        copy->transform.position[0] += 1.0f;
        newObjs.push_back(copy);
    }
    if (selection_.hasSelection()) {
        auto& sel0 = selection_.selection().front();
        bool isGroup = sel0->type == Mc3::ObjectType::Group   ||
                       sel0->type == Mc3::ObjectType::Union   ||
                       sel0->type == Mc3::ObjectType::Difference ||
                       sel0->type == Mc3::ObjectType::Intersection ||
                       !sel0->children.empty();
        if (isGroup) {
            for (auto& o : newObjs) sel0->children.push_back(o);
            selection_.clear();
            for (auto& o : newObjs) selection_.select(o);
            modified_ = true;
            updateWindowTitle(); return;
        }
    }
    for (auto& o : newObjs) document_.objects.push_back(o);
    selection_.clear();
    for (auto& o : newObjs) selection_.select(o);
    modified_ = true;
    updateWindowTitle();
}

void MeshCraftApplication::groupSelected() {
    if (!selection_.hasSelection()) return;
    pushUndo();
    auto prev = selection_.selection();
    size_t insertIdx = document_.objects.size();
    for (const auto& s : prev)
        for (size_t i = 0; i < document_.objects.size(); ++i)
            if (document_.objects[i].get() == s.get()) { insertIdx = std::min(insertIdx, i); break; }

    auto group = std::make_shared<Mc3::Mc3Object>();
    group->type = Mc3::ObjectType::Group;
    static int groupCounter = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Group%d", ++groupCounter);
    group->name = buf;

    for (const auto& s : prev) {
        group->children.push_back(s);
        removeFromList(document_.objects, s.get());
    }
    insertIdx = std::min(insertIdx, document_.objects.size());
    document_.objects.insert(document_.objects.begin() + static_cast<std::ptrdiff_t>(insertIdx), group);
    selection_.clear(); selection_.select(group);
    modified_ = true;
    updateWindowTitle();
}

void MeshCraftApplication::ungroupSelected() {
    if (!selection_.hasSelection()) return;
    auto& sel0 = selection_.selection().front();
    if (sel0->type != Mc3::ObjectType::Group || sel0->children.empty()) return;
    pushUndo();
    auto children = sel0->children;
    auto* parentList = findParentList(document_.objects, sel0.get());
    if (!parentList) return;
    auto it = std::find_if(parentList->begin(), parentList->end(),
        [&](const auto& o) { return o.get() == sel0.get(); });
    if (it == parentList->end()) return;
    auto insertIt = parentList->erase(it);
    for (const auto& child : children) { insertIt = parentList->insert(insertIt, child); ++insertIt; }
    selection_.clear();
    for (auto& child : children) selection_.select(child);
    modified_ = true;
    updateWindowTitle();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
std::vector<const Mc3::Mc3Object*> MeshCraftApplication::selectedPointers() const {
    std::vector<const Mc3::Mc3Object*> result;
    for (const auto& s : selection_.selection())
        result.push_back(s.get());
    return result;
}

void MeshCraftApplication::updateWindowTitle() {
    std::string title = "Mesh Craft";
    if (!document_.model.empty() && document_.model != "unnamed")
        title += " - " + document_.model;
    if (!currentFile_.empty())
        title += " [" + currentFile_.filename().string() + "]";
    if (modified_) title += " *";
    const char* toolNames[] = { "Select","Move","Rotate","Scale","Add Box","Add Sphere","Add Cylinder","Add Cone","Add Plane" };
    title += " | "; title += toolNames[static_cast<int>(activeTool_)];
    getWindowProperty().setTitleProperty(title);
}

// ---------------------------------------------------------------------------
// ImGui UI
// ---------------------------------------------------------------------------
void MeshCraftApplication::pushUndo() {
    undoStack_.push_back(deepCopyDoc(document_));
    if (static_cast<int>(undoStack_.size()) > kUndoMax)
        undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
    sceneRenderer_->clearCsgCache();
}

// ---------------------------------------------------------------------------
// Screenshot
// ---------------------------------------------------------------------------

void MeshCraftApplication::saveScreenshot(const std::string& path) {
    auto& gd = getGraphicsDeviceProperty();
    int w = gd.getViewportProperty().getWidthProperty();
    int h = gd.getViewportProperty().getHeightProperty();
    if (w <= 0 || h <= 0) return;

    using PFNGLFINISH     = void(*)();
    using PFNGLBINDBUFFER = void(*)(unsigned int, unsigned int);
    using PFNGLREADPIXELS = void(*)(int, int, int, int, unsigned int, unsigned int, void*);
    auto fnFinish     = reinterpret_cast<PFNGLFINISH>    (SDL_GL_GetProcAddress("glFinish"));
    auto fnBindBuffer = reinterpret_cast<PFNGLBINDBUFFER>(SDL_GL_GetProcAddress("glBindBuffer"));
    auto fnReadPixels = reinterpret_cast<PFNGLREADPIXELS>(SDL_GL_GetProcAddress("glReadPixels"));
    if (!fnReadPixels) { std::cerr << "[Screenshot] glReadPixels not available\n"; return; }
    if (fnFinish)     fnFinish();
    if (fnBindBuffer) fnBindBuffer(0x88EC, 0);

    constexpr unsigned int GL_RGBA          = 0x1908;
    constexpr unsigned int GL_UNSIGNED_BYTE = 0x1401;
    std::vector<unsigned char> pixels(w * h * 4);
    fnReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int row = h - 1; row >= 0; --row)
        for (int col = 0; col < w; ++col) {
            int idx = (row * w + col) * 4;
            f.write(reinterpret_cast<char*>(&pixels[idx]), 3);
        }
    std::cout << "[Screenshot] written " << path << "\n";
}

// ---------------------------------------------------------------------------
// Misc helpers
// ---------------------------------------------------------------------------

Mc3::Mc3Object* MeshCraftApplication::flatFindById(const std::string& id) const {
    std::function<Mc3::Mc3Object*(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> find;
    find = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) -> Mc3::Mc3Object* {
        for (const auto& obj : list) {
            if (obj->id == id) return obj.get();
            if (!obj->children.empty()) {
                auto* r = find(obj->children);
                if (r) return r;
            }
        }
        return nullptr;
    };
    return find(document_.objects);
}

Mc3::Mc3Object* MeshCraftApplication::flatFindByName(const std::string& name) const {
    std::function<Mc3::Mc3Object*(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> find;
    find = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) -> Mc3::Mc3Object* {
        for (const auto& obj : list) {
            if (obj->name == name) return obj.get();
            if (!obj->children.empty()) {
                auto* r = find(obj->children);
                if (r) return r;
            }
        }
        return nullptr;
    };
    return find(document_.objects);
}

// ---------------------------------------------------------------------------
// Batch rename
// ---------------------------------------------------------------------------

void MeshCraftApplication::batchRenameSelected() {
    auto& sel = selection_.selection();
    if (sel.empty()) return;
    pushUndo();
    int idx = 1;
    for (const auto& s : sel) {
        if (lockedIds_.count(s->id)) { ++idx; continue; }
        std::string newName = applyRenamePattern(batchRenameBuf_, s->name, idx,
                                                  objectTypeName(s->type));
        if (!newName.empty()) s->name = newName;
        ++idx;
    }
    modified_ = true;
    updateWindowTitle();
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Renamed %d object(s)",
                  static_cast<int>(sel.size()));
    setStatusMsg(msg);
}

// ---------------------------------------------------------------------------
// Select parent
// ---------------------------------------------------------------------------

void MeshCraftApplication::selectParent() {
    if (!selection_.hasSelection()) return;
    const Mc3::Mc3Object* target = selection_.selection().front().get();
    Mc3::Mc3Object* parentRaw = findParentObject(document_.objects, target);
    if (!parentRaw) {
        setStatusMsg("Already at root level", false, 1.5f);
        return;
    }
    // Locate the shared_ptr that owns parentRaw so SelectionManager can hold it
    std::function<std::shared_ptr<Mc3::Mc3Object>(
        const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> findSptr;
    findSptr = [&](const auto& list) -> std::shared_ptr<Mc3::Mc3Object> {
        for (const auto& obj : list) {
            if (obj.get() == parentRaw) return obj;
            if (!obj->children.empty()) {
                auto r = findSptr(obj->children);
                if (r) return r;
            }
        }
        return nullptr;
    };
    auto parentSptr = findSptr(document_.objects);
    if (!parentSptr) return;
    selection_.clear();
    selection_.select(parentSptr);
    updateWindowTitle();
    setStatusMsg("Selected parent: " + parentSptr->name, false, 1.5f);
}

// ---------------------------------------------------------------------------
// Randomize Transform
// ---------------------------------------------------------------------------

void MeshCraftApplication::randomizeTransformSelected() {
    auto& sel = selection_.selection();
    if (sel.empty()) return;
    pushUndo();
    std::mt19937 rng{std::random_device{}()};
    auto rand11 = [&]() -> float {
        return std::uniform_real_distribution<float>(-1.0f, 1.0f)(rng);
    };
    int count = 0;
    for (const auto& s : sel) {
        if (lockedIds_.count(s->id)) continue;
        for (int i = 0; i < 3; ++i) {
            if (scatterPosRange_[i] != 0.0f)
                s->transform.position[i] += rand11() * scatterPosRange_[i];
            if (scatterRotRange_[i] != 0.0f)
                s->transform.rotation[i] += rand11() * scatterRotRange_[i];
        }
        if (scatterScaleRange_ != 0.0f) {
            float delta = rand11() * scatterScaleRange_ * 0.01f;
            for (int i = 0; i < 3; ++i)
                s->transform.scale[i] *= (1.0f + delta);
        }
        ++count;
    }
    modified_ = true;
    updateWindowTitle();
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Randomized %d object(s)", count);
    setStatusMsg(msg);
}

// ---------------------------------------------------------------------------
// Find & Replace names
// ---------------------------------------------------------------------------

void MeshCraftApplication::findReplaceNames() {
    const std::string findStr(findBuf_);
    const std::string replStr(replaceBuf_);
    if (findStr.empty()) return;

    // Build lowercase needle for case-insensitive mode
    std::string findLow = findStr;
    if (!findCaseSensitive_)
        for (auto& c : findLow) c = (char)std::tolower((unsigned char)c);

    // Returns modified string, or src unchanged if no match
    auto replaceAll = [&](const std::string& src) -> std::string {
        std::string haystack = src;
        std::string needle   = findStr;
        if (!findCaseSensitive_) {
            haystack.resize(src.size());
            for (size_t i = 0; i < src.size(); ++i)
                haystack[i] = (char)std::tolower((unsigned char)src[i]);
            needle = findLow;
        }
        std::string result;
        size_t lastPos = 0, pos;
        bool found = false;
        while ((pos = haystack.find(needle, lastPos)) != std::string::npos) {
            result += src.substr(lastPos, pos - lastPos);
            result += replStr;
            lastPos = pos + needle.size();
            found = true;
        }
        if (!found) return src;
        result += src.substr(lastPos);
        return result;
    };

    // Count eligible matches (skip locked objects)
    int matchCount = 0;
    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> countWalk;
    countWalk = [&](const auto& list) {
        for (const auto& o : list) {
            if (!lockedIds_.count(o->id)) {
                bool inScope = !findSelectedOnly_ || selection_.isSelected(o.get());
                if (inScope && replaceAll(o->name) != o->name) ++matchCount;
            }
            countWalk(o->children);
        }
    };
    countWalk(document_.objects);

    if (matchCount == 0) {
        setStatusMsg("No matches found for \"" + findStr + "\"", true, 2.5f);
        return;
    }

    pushUndo();
    std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> applyWalk;
    applyWalk = [&](auto& list) {
        for (auto& o : list) {
            if (!lockedIds_.count(o->id)) {
                bool inScope = !findSelectedOnly_ || selection_.isSelected(o.get());
                if (inScope) o->name = replaceAll(o->name);
            }
            applyWalk(o->children);
        }
    };
    applyWalk(document_.objects);

    modified_ = true;
    updateWindowTitle();
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Replaced %d object name(s)", matchCount);
    setStatusMsg(msg);
}

// ---------------------------------------------------------------------------
// Linear Array (array duplicate)
// ---------------------------------------------------------------------------

void MeshCraftApplication::arrayDuplicate() {
    if (!selection_.hasSelection()) return;
    const int   count   = std::max(2, arrayDupCount_);
    const int   axis    = std::clamp(arrayDupAxis_, 0, 2);
    const float spacing = arrayDupSpacing_;

    auto prev = selection_.selection();
    pushUndo();
    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;

    for (const auto& src : prev) {
        auto* parentList = findParentList(document_.objects, src.get());
        if (!parentList) continue;
        auto it = std::find_if(parentList->begin(), parentList->end(),
            [&](const auto& o){ return o.get() == src.get(); });
        if (it == parentList->end()) continue;
        auto insertIt = it + 1;

        const float basePos = src->transform.position[axis];
        for (int i = 1; i < count; ++i) {
            auto copy = deepCopyObject(*src);
            // Unique id/name per copy
            copy->name = src->name + "_" + std::to_string(i);
            copy->id   = src->id + "_arr" + std::to_string(i);
            if (arrayDupRelative_)
                copy->transform.position[axis] = basePos + static_cast<float>(i) * spacing;
            else
                copy->transform.position[axis] = static_cast<float>(i) * spacing;
            insertIt = parentList->insert(insertIt, copy);
            ++insertIt;
            newObjs.push_back(copy);
        }
    }

    if (!newObjs.empty()) {
        // Select originals + all new copies
        selection_.clear();
        for (const auto& src : prev) selection_.select(src);
        for (auto& o : newObjs) selection_.select(o);
        modified_ = true;
        updateWindowTitle();
        char msg[64];
        std::snprintf(msg, sizeof(msg), "Array: %d object(s) created",
                      static_cast<int>(newObjs.size()));
        setStatusMsg(msg);
    }
}

// ---------------------------------------------------------------------------
// Copy Properties to Selected
// ---------------------------------------------------------------------------

void MeshCraftApplication::copyPropsToSelected() {
    const auto& sel = selection_.selection();
    if (sel.size() < 2) { setStatusMsg("Need 2+ objects selected", true, 2.0f); return; }

    const auto& src = *sel.front();
    pushUndo();
    int count = 0;
    for (int i = 1; i < static_cast<int>(sel.size()); ++i) {
        auto& dst = *sel[i];
        if (copyPropsMaterial_)         dst.material         = src.material;
        if (copyPropsMaterialOverride_) dst.materialOverride = src.materialOverride;
        if (copyPropsCollision_)        dst.collision        = src.collision;
        if (copyPropsTags_)             dst.tags             = src.tags;
        if (copyPropsVisibility_)       dst.visible          = src.visible;
        if (copyPropsIsCutter_)         dst.isCutter         = src.isCutter;
        ++count;
    }
    modified_ = true;
    updateWindowTitle();
    char msg[80];
    std::snprintf(msg, sizeof(msg), "Copied properties to %d object(s)", count);
    setStatusMsg(msg);
}

// ---------------------------------------------------------------------------
// Animation helpers
// ---------------------------------------------------------------------------

} // namespace MeshCraft
