#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"

#include <imgui.h>

#include <SDL3/SDL.h>

#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/Viewport.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <numbers>
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
    case Mc3::ObjectType::Capsule:  { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Capsule; p.radius = 0.5f; p.height = 1.0f; p.segments = 16; obj->primitive = p; } break;
    case Mc3::ObjectType::Disk:     { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Disk; p.radius = 0.5f; p.minorRadius = 0.0f; p.segments = 32; obj->primitive = p; } break;
    case Mc3::ObjectType::Grid:      { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Grid; p.size = {1.0f,0.0f,1.0f}; p.subdivisionsX = 4; p.subdivisionsZ = 4; obj->primitive = p; } break;
    case Mc3::ObjectType::IcoSphere: { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::IcoSphere; p.radius = 0.5f; obj->primitive = p; } break;
    // STAB-0721: give freshly-created Areas an editable size, matching what
    // areaType already gets when loaded from XML with a size= attribute
    // (Mc3XmlParser.cpp defaults primitiveType to Box for Area since it has
    // no ObjectType::Area case of its own) -- without this the Properties
    // panel's "Area (trigger zone)" label had nothing to show underneath it.
    case Mc3::ObjectType::Area:     { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Box;      p.size = {1.0f,1.0f,1.0f}; obj->primitive = p; } break;
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
    default: break; // Group, Mesh, Instance — no primitive
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
            updateWindowTitle();
            recordStep("add", {objectTypeName(type)});
            return;
        }
    }
    document_.objects.push_back(obj);
    selection_.clear(); selection_.select(obj);
    modified_ = true;
    updateWindowTitle();
    recordStep("add", {objectTypeName(type)});
}

// (removeFromList is defined in MeshCraftPrivate.hpp)

void MeshCraftApplication::deleteSelected() {
    pushUndo();
    for (const auto& s : selection_.selection()) {
        if (lockedIds_.count(s->id)) continue;
        removeFromListAlg(document_.objects, s.get());
    }
    selection_.clear();
    modified_ = true;
    updateWindowTitle();
    recordStep("delete");
}
void MeshCraftApplication::toggleIsolate() {
    // SYS-W14-16: Mc3Object::visible is a real, persisted document field
    // (not editor-only state), and both branches below mutate it plus set
    // modified_ -- but neither called pushUndo(), so Ctrl+Z right after
    // isolating/un-isolating couldn't restore the pre-toggle visibility.
    // Found by a dedicated undo-coverage audit, not a quick pick.
    pushUndo();
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
    // AUD-031: was a hand-copied duplicate of duplicateObjectsAlg's own
    // document-mutation loop; now delegates to it directly (single tested
    // implementation).
    auto newObjs = duplicateObjectsAlg(document_.objects, prev);

    if (!newObjs.empty()) {
        selection_.clear();
        for (auto& o : newObjs) selection_.select(o);
        modified_ = true;
        updateWindowTitle();
        recordStep("duplicate");
    }
}

void MeshCraftApplication::copySelected() {
    if (!selection_.hasSelection()) return;
    clipboard_.clear();
    for (const auto& s : selection_.selection())
        clipboard_.push_back(deepCopyObjectAlg(*s));
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
        auto copy = deepCopyObjectAlg(*src);
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
    // Naming policy (a session-lifetime counter) stays here -- it's an
    // editor-session concern, not part of the document-mutation logic
    // groupObjectsAlg mirrors.
    static int groupCounter = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Group%d", ++groupCounter);
    // AUD-031: was a hand-copied duplicate of groupObjectsAlg's own
    // document-mutation logic; now delegates to it directly.
    auto group = groupObjectsAlg(document_.objects, prev, buf);
    selection_.clear(); selection_.select(group);
    modified_ = true;
    updateWindowTitle();
    recordStep("group");
}

void MeshCraftApplication::ungroupSelected() {
    if (!selection_.hasSelection()) return;
    auto& sel0 = selection_.selection().front();
    if (sel0->type != Mc3::ObjectType::Group || sel0->children.empty()) return;
    pushUndo();
    // AUD-031: was a hand-copied duplicate of ungroupObjectAlg's own
    // document-mutation logic; now delegates to it directly.
    auto children = ungroupObjectAlg(document_.objects, sel0);
    if (children.empty()) return;
    selection_.clear();
    for (auto& child : children) selection_.select(child);
    modified_ = true;
    updateWindowTitle();
    recordStep("ungroup");
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
    // Bounds-safe, exhaustive mapping (MeshCraft/Editor/ActiveTool.hpp).
    // Previously indexed a 9-element array with the 10-value ActiveTool enum,
    // so the Measure tool read one past the end (undefined behavior).
    title += " | "; title += activeToolName(activeTool_);
    getWindowProperty().setTitleProperty(title);
}

// ---------------------------------------------------------------------------
// ImGui UI
// ---------------------------------------------------------------------------
// SYS-W9-03: ids of the currently selected objects, in selection order.
// Objects with no id (empty string) can't be reliably re-found via
// flatFindSharedById() later -- matching flatFindById's own first-match-
// by-id semantics -- so they're skipped rather than recorded.
std::vector<std::string> MeshCraftApplication::currentSelectionIds() const {
    std::vector<std::string> ids;
    ids.reserve(selection_.selection().size());
    for (const auto& obj : selection_.selection())
        if (obj && !obj->id.empty()) ids.push_back(obj->id);
    return ids;
}

// SYS-W9-03: re-resolves each id against the CURRENT document_ (post-swap)
// and selects the objects found. An id that no longer exists post-swap
// (e.g. undone past the command that created it) is silently skipped --
// falls back toward an empty/partial selection rather than dangling or
// crashing, exactly as the human-authorized decision specified.
void MeshCraftApplication::restoreSelectionByIds(const std::vector<std::string>& ids) {
    selection_.clear();
    for (const auto& id : ids) {
        auto obj = flatFindSharedById(id);
        if (obj) selection_.select(obj);
    }
}

// AUD-031: the push-then-trim-to-cap pattern at all 3 stack mutation sites
// below was hand-copied 3 times; now delegates to pushWithCapAlg.
void MeshCraftApplication::pushUndo() {
    pushWithCapAlg(undoStack_, deepCopyDoc(document_), kUndoMax);
    pushWithCapAlg(undoSelectionStack_, currentSelectionIds(), kUndoMax);
    redoStack_.clear();
    redoSelectionStack_.clear();
    // CSG cache no longer cleared here: hash-based invalidation handles it (K1)
    // SYS-W5-04: called before virtually every mutating command, so this is
    // objectIndex_'s single invalidation choke point for in-place tree edits.
    objectIndex_.invalidate();
}

bool MeshCraftApplication::undoOnActivate(bool widgetChanged) {
    if (ImGui::IsItemActivated()) pushUndo();
    return widgetChanged;
}

// Shared by the keyboard shortcut, the Edit menu, and the command palette
// (STAB-0486) so all 3 entry points stay behaviorally identical instead of
// hand-copied and free to drift.
//
// SYS-W9-03 (human-authorized decision, 2026-07-17): restores the selection
// that was active immediately before the command being undone/redone ran,
// instead of unconditionally clearing it. undoSelectionStack_/
// redoSelectionStack_ are kept in lockstep (index-for-index) with
// undoStack_/redoStack_ by pushUndo()/performUndo()/performRedo() always
// pushing/popping both pairs together.
void MeshCraftApplication::performUndo() {
    if (undoStack_.empty()) return;
    pushWithCapAlg(redoStack_, deepCopyDoc(document_), kUndoMax);
    pushWithCapAlg(redoSelectionStack_, currentSelectionIds(), kUndoMax);
    document_ = std::move(undoStack_.back());
    objectIndex_.invalidate();  // SYS-W5-04: wholesale document_ replacement
    undoStack_.pop_back();
    std::vector<std::string> ids = std::move(undoSelectionStack_.back());
    undoSelectionStack_.pop_back();
    restoreSelectionByIds(ids);
    modified_ = true;
    updateWindowTitle();
    evaluateAndPushAnimOverrides();
}

void MeshCraftApplication::performRedo() {
    if (redoStack_.empty()) return;
    pushWithCapAlg(undoStack_, deepCopyDoc(document_), kUndoMax);
    pushWithCapAlg(undoSelectionStack_, currentSelectionIds(), kUndoMax);
    document_ = std::move(redoStack_.back());
    objectIndex_.invalidate();  // SYS-W5-04: wholesale document_ replacement
    redoStack_.pop_back();
    std::vector<std::string> ids = std::move(redoSelectionStack_.back());
    redoSelectionStack_.pop_back();
    restoreSelectionByIds(ids);
    modified_ = true;
    updateWindowTitle();
    evaluateAndPushAnimOverrides();
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

// SYS-W5-04: delegates to objectIndex_ (Editor::ObjectIndex), a lazily-
// rebuilt id/name -> object cache, instead of a fresh O(n) tree walk per
// call. Signature and first-match-in-document-order semantics unchanged --
// see ObjectIndex.hpp for the invalidation invariant this depends on.
Mc3::Mc3Object* MeshCraftApplication::flatFindById(const std::string& id) const {
    return objectIndex_.findById(document_, id);
}

std::shared_ptr<Mc3::Mc3Object> MeshCraftApplication::flatFindSharedById(const std::string& id) const {
    return objectIndex_.findSharedById(document_, id);
}

Mc3::Mc3Object* MeshCraftApplication::flatFindByName(const std::string& name) const {
    return objectIndex_.findByName(document_, name);
}

// ---------------------------------------------------------------------------
// Batch rename
// ---------------------------------------------------------------------------

void MeshCraftApplication::batchRenameSelected() {
    auto& sel = selection_.selection();
    if (sel.empty()) return;
    pushUndo();
    int renamed = batchRenameObjects(sel, lockedIds_, batchRenameBuf_, &document_.actions);
    modified_ = true;
    updateWindowTitle();
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Renamed %d object(s)", renamed);
    setStatusMsg(msg);
    recordStep("batch_rename", {batchRenameBuf_});
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

void MeshCraftApplication::breakInstance() {
    if (!selection_.hasSelection()) return;
    auto inst = selection_.selection().front();
    if (inst->type != Mc3::ObjectType::Instance) {
        setStatusMsg("Selected object is not an Instance", true, 2.0f);
        return;
    }
    if (!document_.definitions.count(inst->definition)) {
        setStatusMsg("Definition not found: " + inst->definition, true, 2.0f);
        return;
    }

    pushUndo();
    std::string definitionName = inst->definition;
    auto copy = breakInstanceAlg(document_, inst);

    selection_.clear();
    selection_.select(copy);
    modified_ = true; updateWindowTitle();
    setStatusMsg("Instance broken: " + definitionName, false, 2.0f);
}

void MeshCraftApplication::convertToDefinition() {
    if (!selection_.hasSelection()) return;
    auto src = selection_.selection().front();

    pushUndo();
    auto inst = convertToDefinitionAlg(document_, src);

    selection_.clear();
    selection_.select(inst);
    modified_ = true; updateWindowTitle();
    setStatusMsg("Converted to definition: " + inst->definition, false, 2.5f);
}

void MeshCraftApplication::exportSubtreeAsTemplate(const std::string& defName,
                                                   const std::string& filePath) {
    if (!selection_.hasSelection()) return;
    if (defName.empty()) return;

    auto src = selection_.selection().front();
    pushUndo();

    // Deep-copy subtree into definitions map (reset transform to identity)
    auto defObj = deepCopyObjectAlg(*src);
    defObj->id   = defName;
    defObj->name = defName;
    defObj->transform.position = {0.0f, 0.0f, 0.0f};
    defObj->transform.rotation = {0.0f, 0.0f, 0.0f};
    defObj->transform.scale    = {1.0f, 1.0f, 1.0f};
    document_.definitions[defName] = defObj;

    // Optionally save the subtree to a file — carries along any materials/
    // textures the subtree references, so the standalone file doesn't
    // silently lose its appearance when loaded elsewhere (STAB-0536).
    if (!filePath.empty()) {
        Mc3::Mc3Document tmp = exportSubtreeTemplateAlg(document_, defName, defObj);
        tmp.saveToFile(filePath);
    }

    // Replace original with an Instance pointing to the new definition
    auto inst = std::make_shared<Mc3::Mc3Object>();
    inst->id         = src->id;
    inst->name       = src->name.empty() ? defName : src->name;
    inst->type       = Mc3::ObjectType::Instance;
    inst->definition = defName;
    inst->transform  = src->transform;
    inst->visible    = src->visible;
    inst->layer      = src->layer;
    inst->tags       = src->tags;

    auto* parentList = findParentListAlg(document_.objects, src.get());
    if (parentList) {
        for (auto& obj : *parentList) {
            if (obj.get() == src.get()) { obj = inst; break; }
        }
    } else {
        document_.objects.push_back(inst);
    }

    selection_.clear();
    selection_.select(inst);
    modified_ = true; updateWindowTitle();
    std::string msg = "Exported as template: " + defName;
    if (!filePath.empty()) msg += " → " + filePath;
    setStatusMsg(msg, false, 3.0f);
}

void MeshCraftApplication::alignToObject() {
    if (selection_.selection().size() < 2) return;
    const std::string& srcName = selection_.selection().front()->name;
    pushUndo();
    int aligned = alignToObjectAlg(selection_.selection(), lockedIds_);
    modified_ = true; updateWindowTitle();
    setStatusMsg("Aligned " + std::to_string(aligned) +
                 " object(s) to " + srcName, false, 2.0f);
}

// Shared by the Edit menu and the command palette (STAB-0486) so both stay
// behaviorally identical instead of hand-copied and free to drift.
void MeshCraftApplication::dropSelectedToGroundPlane() {
    if (!selection_.hasSelection()) return;
    pushUndo();
    int dropped = 0;
    for (const auto& s : selection_.selection()) {
        if (lockedIds_.count(s->id)) continue;
        float bottomOffset = 0.0f; // distance from pivot to lowest point
        if (s->primitive) {
            const auto& p = *s->primitive;
            float sy = std::abs(s->transform.scale[1]);
            switch (p.primitiveType) {
                case Mc3::PrimitiveType::Box:
                case Mc3::PrimitiveType::Cube:   bottomOffset = (p.size[1]*sy)/2.0f; break;
                case Mc3::PrimitiveType::Sphere: bottomOffset = p.radius*sy;         break;
                case Mc3::PrimitiveType::Cylinder:
                case Mc3::PrimitiveType::Cone:   bottomOffset = (p.height/2.0f)*sy; break;
                default: bottomOffset = 0.0f; break;
            }
        }
        s->transform.position[1] = bottomOffset;
        ++dropped;
    }
    modified_ = true; updateWindowTitle();
    setStatusMsg("Dropped " + std::to_string(dropped) + " object(s) to ground plane");
}

// H14: Scale selected objects around their group center
void MeshCraftApplication::groupScaleSelected() {
    const auto& sel = selection_.selection();
    if (sel.empty()) return;
    const float f = groupScaleFactor_;

    pushUndo();
    int scaled = groupScaleAlg(sel, lockedIds_, f);
    modified_ = true; updateWindowTitle();
    setStatusMsg("Group scale ×" + std::to_string(f).substr(0, 5) +
                 " on " + std::to_string(scaled) + " object(s)", false, 2.0f);
    recordStep("group_scale", {std::to_string(f)});
}

void MeshCraftApplication::selectChildren() {
    if (!selection_.hasSelection()) return;
    const auto& sel0 = selection_.selection().front();
    if (sel0->children.empty()) {
        setStatusMsg("No children", false, 1.5f);
        return;
    }
    selection_.clear();
    for (auto& c : flattenDescendantsAlg(sel0->children)) selection_.select(c);
    updateWindowTitle();
    setStatusMsg("Selected " + std::to_string(selection_.selection().size()) + " child object(s)", false, 1.5f);
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

    // Build selected-ID set for selectedOnly mode
    std::set<std::string> selectedIds;
    if (findSelectedOnly_)
        for (const auto& s : selection_.selection()) selectedIds.insert(s->id);

    int matchCount = countFindReplaceMatches(document_.objects, lockedIds_,
                                             findStr, replStr, findCaseSensitive_,
                                             findSelectedOnly_, selectedIds);
    if (matchCount == 0) {
        setStatusMsg("No matches found for \"" + findStr + "\"", true, 2.5f);
        return;
    }

    pushUndo();
    applyFindReplaceNames(document_.objects, lockedIds_,
                          findStr, replStr, findCaseSensitive_,
                          findSelectedOnly_, selectedIds);
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
    auto prev = selection_.selection();
    pushUndo();

    auto newObjs = arrayDuplicateObjects(document_.objects, prev,
                                          arrayDupCount_, arrayDupAxis_,
                                          arrayDupSpacing_, arrayDupRelative_);
    if (!newObjs.empty()) {
        selection_.clear();
        for (const auto& src : prev) selection_.select(src);
        for (auto& o : newObjs) selection_.select(o);
        modified_ = true;
        updateWindowTitle();
        char msg[64];
        std::snprintf(msg, sizeof(msg), "Array: %d object(s) created",
                      static_cast<int>(newObjs.size()));
        setStatusMsg(msg);
        recordStep("linear_array", {std::to_string(std::max(2, arrayDupCount_)),
                                    std::to_string(std::clamp(arrayDupAxis_, 0, 2)),
                                    std::to_string(arrayDupSpacing_)});
    }
}

// ---------------------------------------------------------------------------
// H4: Scatter Along Curve
// ---------------------------------------------------------------------------
void MeshCraftApplication::scatterAlongCurve() {
    if (!selection_.hasSelection() || scatterCurveCount_ < 2) return;

    auto prev = selection_.selection();
    pushUndo();

    // Jitter seed per call
    unsigned seed = static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto rng = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
    };

    ScatterCurveParamsAlg params;
    params.count       = scatterCurveCount_;
    params.mode        = scatterCurveMode_;
    params.axis        = scatterCurveAxis_;
    params.spacing     = scatterCurveSpacing_;
    params.arcAngleDeg = scatterCurveArcAngle_;
    params.radius      = scatterCurveRadius_;
    params.jitter      = scatterCurveJitter_;

    auto newObjs = scatterAlongCurveAlg(document_.objects, prev, params, rng);

    if (!newObjs.empty()) {
        selection_.clear();
        for (const auto& s : prev) selection_.select(s);
        for (auto& o : newObjs) selection_.select(o);
        modified_ = true; updateWindowTitle();
        setStatusMsg("Scattered " + std::to_string(newObjs.size()) + " copies along curve");
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

// ---------------------------------------------------------------------------
// H5 — Reset pivot to (0,0,0) while keeping geometry in world space
// ---------------------------------------------------------------------------

void MeshCraftApplication::resetPivot() {
    using namespace Microsoft::Xna::Framework;
    if (!selection_.hasSelection()) return;
    pushUndo();
    const float deg = std::numbers::pi_v<float> / 180.0f;
    for (const auto& s : selection_.selection()) {
        if (lockedIds_.count(s->id)) continue;
        const auto& p = s->transform.pivot;
        const auto& rot = s->transform.rotation;
        // Compensation: pos_new = pos + d*R - d  where d = -pivot (zeroing pivot)
        float dX = -p[0], dY = -p[1], dZ = -p[2];
        Matrix R = Matrix::CreateFromYawPitchRoll(rot[1]*deg, rot[0]*deg, rot[2]*deg);
        float rX = dX*R.M11 + dY*R.M21 + dZ*R.M31;
        float rY = dX*R.M12 + dY*R.M22 + dZ*R.M32;
        float rZ = dX*R.M13 + dY*R.M23 + dZ*R.M33;
        s->transform.position[0] += rX - dX;
        s->transform.position[1] += rY - dY;
        s->transform.position[2] += rZ - dZ;
        s->transform.pivot = {0.0f, 0.0f, 0.0f};
    }
    modified_ = true;
    updateWindowTitle();
    setStatusMsg("Pivot reset to origin", false, 1.5f);
}

} // namespace MeshCraft
