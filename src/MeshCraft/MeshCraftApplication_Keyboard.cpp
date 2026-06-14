#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <numbers>
#include <string>

namespace MeshCraft {

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Graphics;

void MeshCraftApplication::handleKeyboardShortcuts(const KeyboardState& ks, const KeyboardState& prevKs) {
    bool ctrl  = ks.IsKeyDown(Keys::LeftControl)  || ks.IsKeyDown(Keys::RightControl);
    bool shift = ks.IsKeyDown(Keys::LeftShift)    || ks.IsKeyDown(Keys::RightShift);
    bool alt   = ks.IsKeyDown(Keys::LeftAlt)      || ks.IsKeyDown(Keys::RightAlt);

    // Escape: deselect / reset tool / exit
    if (justPressed(ks, prevKs, Keys::Escape)) {
        if (activeTool_ != ActiveTool::Select || selection_.hasSelection()) {
            activeTool_ = ActiveTool::Select;
            selection_.clear();
            updateWindowTitle();
        } else {
            confirmIfModified(PendingAction::ExitApp);
        }
        return;
    }

    // Undo / Redo
    if (ctrl && justPressed(ks, prevKs, Keys::Z)) {
        if (!undoStack_.empty()) {
            redoStack_.push_back(deepCopyDoc(document_));
            if (static_cast<int>(redoStack_.size()) > kUndoMax)
                redoStack_.erase(redoStack_.begin());
            document_ = std::move(undoStack_.back());
            undoStack_.pop_back();
            selection_.clear();
            modified_ = true;
            updateWindowTitle();
            evaluateAndPushAnimOverrides();
        }
        return;
    }
    if (ctrl && justPressed(ks, prevKs, Keys::Y)) {
        if (!redoStack_.empty()) {
            undoStack_.push_back(deepCopyDoc(document_));
            if (static_cast<int>(undoStack_.size()) > kUndoMax)
                undoStack_.erase(undoStack_.begin());
            document_ = std::move(redoStack_.back());
            redoStack_.pop_back();
            selection_.clear();
            modified_ = true;
            updateWindowTitle();
            evaluateAndPushAnimOverrides();
        }
        return;
    }

    // Walk mode toggle (F5)
    if (justPressed(ks, prevKs, Keys::F5)) {
        if (walkModeEnabled_) exitWalkMode(); else enterWalkMode();
        return;
    }

    // Screenshot (F11)
    if (justPressed(ks, prevKs, Keys::F11)) {
        saveScreenshot("screenshot.ppm");
        std::cout << "[MeshCraft] Screenshot saved to screenshot.ppm\n";
        return;
    }

    // File ops
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::N)) { confirmIfModified(PendingAction::NewScene);  return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::O)) { confirmIfModified(PendingAction::OpenFile);  return; }
    if (ctrl &&  shift && justPressed(ks, prevKs, Keys::S)) { saveFileAs(); return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::S)) { saveFile();   return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::E)) { exportGltf(); return; }

    // Tool selection
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::G)) { activeTool_ = ActiveTool::Move;   updateWindowTitle(); }
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::R)) { activeTool_ = ActiveTool::Rotate; updateWindowTitle(); }
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::S)) { activeTool_ = ActiveTool::Scale;  updateWindowTitle(); }
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::Q)) { activeTool_ = ActiveTool::Select; updateWindowTitle(); }

    // Edge overlay toggle
    if (alt && justPressed(ks, prevKs, Keys::W)) { showEdgeOverlay_ = !showEdgeOverlay_; return; }

    // Find & Replace names (Ctrl+H)
    if (ctrl && !alt && justPressed(ks, prevKs, Keys::H)) {
        findReplaceOpen_ = true;
        return;
    }

    // Hide selected (H) / show all hidden (Alt+H)
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::H)) {
        auto sel = selection_.selection();
        if (!sel.empty()) {
            pushUndo();
            for (auto& s : sel) { s->visible = false; }
            selection_.clear();
            modified_ = true; updateWindowTitle();
        }
        return;
    }
    if (!ctrl && alt && justPressed(ks, prevKs, Keys::H)) {
        std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> showAll;
        showAll = [&](auto& list) {
            for (auto& o : list) { o->visible = true; showAll(o->children); }
        };
        pushUndo();
        showAll(document_.objects);
        modified_ = true; updateWindowTitle();
        return;
    }

    // Reset transforms (Alt+G/R/S — position/rotation/scale to default)
    if (!ctrl && alt && justPressed(ks, prevKs, Keys::G)) {
        if (selection_.hasSelection()) {
            pushUndo();
            for (const auto& s : selection_.selection()) {
                if (lockedIds_.count(s->id)) continue;
                s->transform.position = {0.0f, 0.0f, 0.0f};
            }
            modified_ = true; updateWindowTitle();
        }
        return;
    }
    if (!ctrl && alt && justPressed(ks, prevKs, Keys::R)) {
        if (selection_.hasSelection()) {
            pushUndo();
            for (const auto& s : selection_.selection()) {
                if (lockedIds_.count(s->id)) continue;
                s->transform.rotation = {0.0f, 0.0f, 0.0f};
            }
            modified_ = true; updateWindowTitle();
        }
        return;
    }
    if (!ctrl && alt && justPressed(ks, prevKs, Keys::S)) {
        if (selection_.hasSelection()) {
            pushUndo();
            for (const auto& s : selection_.selection()) {
                if (lockedIds_.count(s->id)) continue;
                s->transform.scale = {1.0f, 1.0f, 1.0f};
            }
            modified_ = true; updateWindowTitle();
        }
        return;
    }

    // Copy / paste transform (Ctrl+Shift+C / Ctrl+Shift+V)
    if (ctrl && shift && justPressed(ks, prevKs, Keys::C)) {
        if (selection_.hasSelection()) {
            const auto& src = selection_.selection().front();
            transformClipboard_.position = src->transform.position;
            transformClipboard_.rotation = src->transform.rotation;
            transformClipboard_.scale    = src->transform.scale;
            transformClipboard_.valid    = true;
            setStatusMsg("Transform copied from \"" + src->name + "\"");
        }
        return;
    }
    if (ctrl && shift && justPressed(ks, prevKs, Keys::V)) {
        if (transformClipboard_.valid && selection_.hasSelection()) {
            pushUndo();
            for (const auto& s : selection_.selection()) {
                if (lockedIds_.count(s->id)) continue;
                s->transform.position = transformClipboard_.position;
                s->transform.rotation = transformClipboard_.rotation;
                s->transform.scale    = transformClipboard_.scale;
            }
            modified_ = true; updateWindowTitle();
            setStatusMsg("Transform pasted to " + std::to_string(selection_.selection().size()) + " object(s)");
        } else if (!transformClipboard_.valid) {
            setStatusMsg("Transform clipboard is empty — copy first with Ctrl+Shift+C", true);
        }
        return;
    }

    // Command palette (Ctrl+P)
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::P)) {
        cmdPaletteOpen_ = true;
        return;
    }

    // Copy Properties to Selected (Ctrl+Shift+P)
    if (ctrl && shift && justPressed(ks, prevKs, Keys::P)) {
        if (selection_.selection().size() >= 2) copyPropsOpen_ = true;
        return;
    }

    // Batch rename (Ctrl+Shift+R)
    if (ctrl && shift && justPressed(ks, prevKs, Keys::R)) {
        if (selection_.hasSelection()) batchRenameOpen_ = true;
        return;
    }

    // Isolate selection (Alt+I) — hide all non-selected; toggle again to restore
    if (!ctrl && alt && justPressed(ks, prevKs, Keys::I)) {
        toggleIsolate();
        return;
    }

    // Lock / unlock selected (Ctrl+L)
    if (ctrl && justPressed(ks, prevKs, Keys::L)) {
        for (const auto& s : selection_.selection()) {
            if (lockedIds_.count(s->id)) lockedIds_.erase(s->id);
            else                          lockedIds_.insert(s->id);
        }
        return;
    }

    // Timeline toggle
    if (ctrl && justPressed(ks, prevKs, Keys::T)) { showTimeline_ = !showTimeline_; return; }

    // Animation play/pause (Space)
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::Space)) {
        if (!currentActionName_.empty() && document_.actions.count(currentActionName_)) {
            animPlaying_ = !animPlaying_;
        }
        return;
    }

    // Preset camera views (Numpad)
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad1)) { camera_.yaw = 0.0f;                                camera_.pitch = 0.0f;  return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad3)) { camera_.yaw = std::numbers::pi_v<float> * 0.5f;  camera_.pitch = 0.0f;  return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad5)) { camera_.yaw = std::numbers::pi_v<float>;          camera_.pitch = 0.0f;  return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad7)) { camera_.yaw = 0.0f;                                camera_.pitch = 1.47f; return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad9)) { camera_.yaw = 0.0f;                                camera_.pitch =-1.47f; return; }

    // Camera bookmarks: Ctrl+F1-F5 save, F6-F10 restore
    if (ctrl && justPressed(ks, prevKs, Keys::F1)) { saveCameraBookmark(0); return; }
    if (ctrl && justPressed(ks, prevKs, Keys::F2)) { saveCameraBookmark(1); return; }
    if (ctrl && justPressed(ks, prevKs, Keys::F3)) { saveCameraBookmark(2); return; }
    if (ctrl && justPressed(ks, prevKs, Keys::F4)) { saveCameraBookmark(3); return; }
    if (ctrl && justPressed(ks, prevKs, Keys::F5)) { saveCameraBookmark(4); return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F6))  { restoreCameraBookmark(0); return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F7))  { restoreCameraBookmark(1); return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F8))  { restoreCameraBookmark(2); return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F9))  { restoreCameraBookmark(3); return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F10)) { restoreCameraBookmark(4); return; }

    // Add primitives
    if (!ctrl && justPressed(ks, prevKs, Keys::F1)) { addPrimitive(Mc3::ObjectType::Box);      return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F2)) { addPrimitive(Mc3::ObjectType::Sphere);   return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F3)) { addPrimitive(Mc3::ObjectType::Cylinder); return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F4)) { addPrimitive(Mc3::ObjectType::Cone);     return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F5)) { addPrimitive(Mc3::ObjectType::Plane);    return; }

    // Delete selected
    if (justPressed(ks, prevKs, Keys::Delete)) { deleteSelected(); return; }

    // Select parent (P)
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::P)) { selectParent(); return; }

    // Camera: F = focus on selection or reset
    if (!ctrl && justPressed(ks, prevKs, Keys::F)) {
        if (selection_.hasSelection()) {
            float bMinX = 1e30f, bMinY = 1e30f, bMinZ = 1e30f;
            float bMaxX = -1e30f, bMaxY = -1e30f, bMaxZ = -1e30f;
            for (const auto& s : selection_.selection()) {
                float px = s->transform.position[0];
                float py = s->transform.position[1];
                float pz = s->transform.position[2];
                float hx = 1.0f, hy = 1.0f, hz = 1.0f;
                if (s->primitive) {
                    const auto& p = *s->primitive;
                    float sx = std::abs(s->transform.scale[0]);
                    float sy = std::abs(s->transform.scale[1]);
                    float sz = std::abs(s->transform.scale[2]);
                    hx = hy = hz = 0.5f;
                    switch (p.primitiveType) {
                    case Mc3::PrimitiveType::Box:
                    case Mc3::PrimitiveType::Cube:
                        hx = p.size[0] * 0.5f * sx; hy = p.size[1] * 0.5f * sy; hz = p.size[2] * 0.5f * sz;
                        break;
                    case Mc3::PrimitiveType::Sphere:
                        hx = hy = hz = p.radius * std::max({sx,sy,sz});
                        break;
                    case Mc3::PrimitiveType::Cylinder:
                    case Mc3::PrimitiveType::Cone:
                        hx = hz = p.radius * std::max(sx, sz); hy = p.height * 0.5f * sy;
                        break;
                    case Mc3::PrimitiveType::Plane:
                        hx = p.size[0] * 0.5f * sx; hy = 0.05f; hz = p.size[2] * 0.5f * sz;
                        break;
                    }
                }
                bMinX = std::min(bMinX, px - hx); bMaxX = std::max(bMaxX, px + hx);
                bMinY = std::min(bMinY, py - hy); bMaxY = std::max(bMaxY, py + hy);
                bMinZ = std::min(bMinZ, pz - hz); bMaxZ = std::max(bMaxZ, pz + hz);
            }
            float cx = (bMinX + bMaxX) * 0.5f;
            float cy = (bMinY + bMaxY) * 0.5f;
            float cz = (bMinZ + bMaxZ) * 0.5f;
            float radius = std::max({bMaxX-bMinX, bMaxY-bMinY, bMaxZ-bMinZ}) * 0.5f;
            camera_.focusOn(cx, cy, cz, std::max(radius, 0.5f));
        } else {
            camera_.reset();
        }
        return;
    }

    // Select all
    if (ctrl && justPressed(ks, prevKs, Keys::A)) {
        if (selection_.hasSelection()) {
            auto& sel0 = selection_.selection().front();
            bool isGroup = sel0->type == Mc3::ObjectType::Group   ||
                           sel0->type == Mc3::ObjectType::Union   ||
                           sel0->type == Mc3::ObjectType::Difference ||
                           sel0->type == Mc3::ObjectType::Intersection ||
                           !sel0->children.empty();
            if (isGroup && !sel0->children.empty()) {
                selection_.clear();
                for (auto& child : sel0->children) selection_.select(child);
                updateWindowTitle();
                return;
            }
        }
        selection_.clear();
        for (auto& o : document_.objects) selection_.select(o);
        updateWindowTitle();
        return;
    }

    if (ctrl && justPressed(ks, prevKs, Keys::I)) {
        std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> invertWalk;
        invertWalk = [&](auto& list) {
            for (auto& o : list) {
                if (selection_.isSelected(o.get())) selection_.deselect(o);
                else                                 selection_.select(o);
                invertWalk(o->children);
            }
        };
        invertWalk(document_.objects);
        updateWindowTitle();
        return;
    }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::D)) { duplicateSelected(); return; }
    if (ctrl &&  shift && justPressed(ks, prevKs, Keys::D)) {
        duplicateSelected();
        for (const auto& s : selection_.selection())
            s->transform.position[0] += gridSpacing_;
        char dbuf[64];
        std::snprintf(dbuf, sizeof(dbuf), "Duplicated %d object(s) (+%.4g u on X)",
                      static_cast<int>(selection_.selection().size()), gridSpacing_);
        setStatusMsg(dbuf);
        return;
    }
    if (ctrl && justPressed(ks, prevKs, Keys::C)) { copySelected();   return; }
    if (ctrl && justPressed(ks, prevKs, Keys::X)) { cutSelected();    return; }
    if (ctrl && justPressed(ks, prevKs, Keys::V)) { pasteClipboard(); return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::G)) { groupSelected();   return; }
    if (ctrl &&  shift && justPressed(ks, prevKs, Keys::G)) { ungroupSelected(); return; }

    // Reorder in hierarchy (Ctrl+Up / Ctrl+Down — move object among siblings)
    if (ctrl && !shift && selection_.hasSelection() &&
        (justPressed(ks, prevKs, Keys::Up) || justPressed(ks, prevKs, Keys::Down)))
    {
        bool moveUp = justPressed(ks, prevKs, Keys::Up);
        auto* target = selection_.selection().front().get();
        auto* parentList = findParentList(document_.objects, target);
        if (parentList && parentList->size() > 1) {
            auto it = std::find_if(parentList->begin(), parentList->end(),
                [target](const auto& p) { return p.get() == target; });
            if (it != parentList->end()) {
                if (moveUp && it != parentList->begin()) {
                    pushUndo();
                    std::iter_swap(it, std::prev(it));
                    modified_ = true;
                } else if (!moveUp && std::next(it) != parentList->end()) {
                    pushUndo();
                    std::iter_swap(it, std::next(it));
                    modified_ = true;
                }
            }
        }
        return;
    }

    // Nudge selected objects
    if (!selection_.hasSelection()) return;
    float nudge = (shift ? 0.1f : 1.0f);
    bool nudged = false;
    bool anyNudgePressed =
        justPressed(ks, prevKs, Keys::Left)  || justPressed(ks, prevKs, Keys::Right) ||
        justPressed(ks, prevKs, Keys::Up)    || justPressed(ks, prevKs, Keys::Down)  ||
        justPressed(ks, prevKs, Keys::PageUp)|| justPressed(ks, prevKs, Keys::PageDown);
    if (anyNudgePressed) pushUndo();
    for (auto& selObj : selection_.selection()) {
        auto* obj = selObj.get();
        if (lockedIds_.count(obj->id)) continue;
        if (justPressed(ks, prevKs, Keys::Left))     { obj->transform.position[0] -= nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::Right))    { obj->transform.position[0] += nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::Up))       { obj->transform.position[1] += nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::Down))     { obj->transform.position[1] -= nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::PageUp))   { obj->transform.position[2] -= nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::PageDown)) { obj->transform.position[2] += nudge; nudged = true; }
    }
    if (nudged) { modified_ = true; updateWindowTitle(); }
}

// ---------------------------------------------------------------------------
// Mouse input
// ---------------------------------------------------------------------------


} // namespace MeshCraft
