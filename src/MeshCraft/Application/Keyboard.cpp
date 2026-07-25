#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include "MeshCraft/MeshCraftPrivate.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"

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

namespace MeshCraft::Application {

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
    if (keybindings_.shortcutFired("edit.undo", ks, prevKs)) { performUndo(); return; }
    if (keybindings_.shortcutFired("edit.redo", ks, prevKs)) { performRedo(); return; }

    // Walk mode toggle (F5)
    if (justPressed(ks, prevKs, Keys::F5)) {
        if (walkController_.isActive()) exitWalkMode(); else enterWalkMode();
        return;
    }

    // Screenshot (F11)
    if (justPressed(ks, prevKs, Keys::F11)) {
        saveScreenshot("screenshot.ppm");
        std::cout << "[MeshCraft] Screenshot saved to screenshot.ppm\n";
        return;
    }

    // File ops
    if (keybindings_.shortcutFired("file.new",    ks, prevKs)) { confirmIfModified(PendingAction::NewScene); return; }
    if (keybindings_.shortcutFired("file.open",   ks, prevKs)) { confirmIfModified(PendingAction::OpenFile); return; }
    if (keybindings_.shortcutFired("file.saveAs", ks, prevKs)) { saveFileAs(); return; }
    if (keybindings_.shortcutFired("file.save",   ks, prevKs)) { saveFile();   return; }
    if (keybindings_.shortcutFired("file.export", ks, prevKs)) { exportGltf(); return; }

    // Tool selection
    if (keybindings_.shortcutFired("tool.move",   ks, prevKs)) { activeTool_ = ActiveTool::Move;   updateWindowTitle(); }
    if (keybindings_.shortcutFired("tool.rotate", ks, prevKs)) { activeTool_ = ActiveTool::Rotate; updateWindowTitle(); }
    if (keybindings_.shortcutFired("tool.scale",  ks, prevKs)) { activeTool_ = ActiveTool::Scale;  updateWindowTitle(); }
    if (keybindings_.shortcutFired("tool.select", ks, prevKs)) { activeTool_ = ActiveTool::Select; updateWindowTitle(); }

    // Edge overlay toggle
    if (keybindings_.shortcutFired("view.edgeOverlay", ks, prevKs)) { showEdgeOverlay_ = !showEdgeOverlay_; return; }

    // Find & Replace names
    if (keybindings_.shortcutFired("edit.findReplace", ks, prevKs)) { findReplaceOpen_ = true; return; }

    // Hide selected / show all hidden
    if (keybindings_.shortcutFired("view.hideSelected", ks, prevKs)) {
        auto sel = selection_.selection();
        if (!sel.empty()) {
            pushUndo();
            for (auto& s : sel) { s->visible = false; }
            selection_.clear();
            modified_ = true; updateWindowTitle();
        }
        return;
    }
    if (keybindings_.shortcutFired("view.showAll", ks, prevKs)) {
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
                if (objectLockState_.isLocked(s->id)) continue;
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
                if (objectLockState_.isLocked(s->id)) continue;
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
                if (objectLockState_.isLocked(s->id)) continue;
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
            transformClipboard_.copyFrom(*src);
            setStatusMsg("Transform copied from \"" + src->name + "\"");
        }
        return;
    }
    if (ctrl && shift && justPressed(ks, prevKs, Keys::V)) {
        if (transformClipboard_.hasValue() && selection_.hasSelection()) {
            pushUndo();
            for (const auto& s : selection_.selection()) {
                if (objectLockState_.isLocked(s->id)) continue;
                transformClipboard_.pasteTo(*s);
            }
            modified_ = true; updateWindowTitle();
            setStatusMsg("Transform pasted to " + std::to_string(selection_.selection().size()) + " object(s)");
        } else if (!transformClipboard_.hasValue()) {
            setStatusMsg("Transform clipboard is empty — copy first with Ctrl+Shift+C", true);
        }
        return;
    }

    // Command palette
    if (keybindings_.shortcutFired("ui.cmdPalette", ks, prevKs)) { cmdPaletteOpen_ = true; return; }

    // Copy Properties to Selected (Ctrl+Shift+P — not in keybind table, keep direct)
    if (ctrl && shift && justPressed(ks, prevKs, Keys::P)) {
        if (selection_.selection().size() >= 2) copyPropsOpen_ = true;
        return;
    }

    // Batch rename
    if (keybindings_.shortcutFired("edit.batchRename", ks, prevKs)) {
        if (selection_.hasSelection()) batchRenameOpen_ = true;
        return;
    }

    // Isolate selection
    if (keybindings_.shortcutFired("view.isolate", ks, prevKs)) { toggleIsolate(); return; }

    // Lock / unlock selected
    if (keybindings_.shortcutFired("edit.lock", ks, prevKs)) {
        for (const auto& s : selection_.selection()) {
            objectLockState_.toggle(s->id);
        }
        return;
    }

    // Timeline toggle
    if (keybindings_.shortcutFired("view.timeline", ks, prevKs)) { showTimeline_ = !showTimeline_; return; }

    // Animation play/pause
    if (keybindings_.shortcutFired("anim.playPause", ks, prevKs)) {
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
    if (keybindings_.shortcutFired("edit.delete", ks, prevKs)) { deleteSelected(); return; }

    // Select parent (P)
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::P)) { selectParent(); return; }

    // Camera: focus on selection or reset
    if (keybindings_.shortcutFired("view.focus", ks, prevKs)) {
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
                    case Mc3::PrimitiveType::IcoSphere:
                        hx = hy = hz = p.radius * std::max({sx,sy,sz});
                        break;
                    case Mc3::PrimitiveType::Cylinder:
                    case Mc3::PrimitiveType::Cone:
                        hx = hz = p.radius * std::max(sx, sz); hy = p.height * 0.5f * sy;
                        break;
                    case Mc3::PrimitiveType::Plane:
                    case Mc3::PrimitiveType::Grid:
                        hx = p.size[0] * 0.5f * sx; hy = 0.05f; hz = p.size[2] * 0.5f * sz;
                        break;
                    case Mc3::PrimitiveType::Torus:
                        // AUD-054: was falling through to the generic 0.5f
                        // default above (a -Wswitch warning caught this) --
                        // outer ring radius in X/Z, half tube thickness in Y.
                        hx = hz = (p.majorRadius + p.minorRadius) * std::max(sx, sz);
                        hy = p.minorRadius * sy;
                        break;
                    case Mc3::PrimitiveType::Capsule:
                        // Cylindrical body plus a hemispherical cap of
                        // `radius` on each end along the height axis.
                        hx = hz = p.radius * std::max(sx, sz);
                        hy = (p.height * 0.5f + p.radius) * sy;
                        break;
                    case Mc3::PrimitiveType::Disk:
                        // Flat like Plane, but round -- radius in X/Z, same
                        // thin-height convention as Plane.
                        hx = hz = p.radius * std::max(sx, sz);
                        hy = 0.05f;
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
    if (keybindings_.shortcutFired("edit.selectAll", ks, prevKs)) {
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

    if (keybindings_.shortcutFired("edit.invertSel", ks, prevKs)) {
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
    if (keybindings_.shortcutFired("edit.duplicate", ks, prevKs) && !shift) { duplicateSelected(); return; }
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
    if (keybindings_.shortcutFired("edit.copy",    ks, prevKs)) { copySelected();   return; }
    if (keybindings_.shortcutFired("edit.cut",     ks, prevKs)) { cutSelected();    return; }
    if (keybindings_.shortcutFired("edit.paste",   ks, prevKs)) { pasteClipboard(); return; }
    if (keybindings_.shortcutFired("edit.group",   ks, prevKs)) { groupSelected();   return; }
    if (keybindings_.shortcutFired("edit.ungroup", ks, prevKs)) { ungroupSelected(); return; }

    // Reorder in hierarchy (Ctrl+Up / Ctrl+Down — move object among siblings)
    if (ctrl && !shift && selection_.hasSelection() &&
        (justPressed(ks, prevKs, Keys::Up) || justPressed(ks, prevKs, Keys::Down)))
    {
        bool moveUp = justPressed(ks, prevKs, Keys::Up);
        auto* target = selection_.selection().front().get();
        auto* parentList = findParentListAlg(document_.objects, target);
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
        if (objectLockState_.isLocked(obj->id)) continue;
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


} // namespace MeshCraft::Application
