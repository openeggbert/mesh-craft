#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"

#include <imgui.h>

#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/Viewport.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <set>
#include <string>

namespace MeshCraft {

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Graphics;


float MeshCraftApplication::drawMenuBar()
{
    // -----------------------------------------------------------------------
    // Main menu bar
    // -----------------------------------------------------------------------
    float menuBarH = 0.0f;

    // Generic recursive walk: fn receives each shared_ptr by ref
    auto walkAll = [&](this auto& self, auto& list, auto&& fn) -> void {
        for (auto& o : list) { fn(o); self(o->children, fn); }
    };
    // Select all objects passing pred, clear selection first, update title after
    auto selectBy = [&](auto&& pred) {
        selection_.clear();
        walkAll(document_.objects, [&](const auto& o) { if (pred(o)) selection_.select(o); });
        updateWindowTitle();
    };

    if (ImGui::BeginMainMenuBar()) {
        menuBarH = ImGui::GetWindowHeight();
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New",     "Ctrl+N")) confirmIfModified(PendingAction::NewScene);
            if (ImGui::MenuItem("Open...", "Ctrl+O")) confirmIfModified(PendingAction::OpenFile);
            if (ImGui::BeginMenu("Open Recent", !recentFiles_.empty())) {
                for (int i = 0; i < static_cast<int>(recentFiles_.size()); ++i) {
                    const auto& rf = recentFiles_[static_cast<size_t>(i)];
                    std::string label = rf.filename().string() + "##rf" + std::to_string(i);
                    if (ImGui::MenuItem(label.c_str()))
                        confirmIfModified(PendingAction::OpenRecentFile, rf);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", rf.string().c_str());
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Recent")) {
                    recentFiles_.clear();
                    saveRecentFiles();
                }
                ImGui::EndMenu();
            }
            // STAB-0717: previously OBJ was only reachable indirectly, as a
            // per-object mesh-source path typed into an existing Mesh
            // object's Properties panel field (PropertiesPanel.cpp's
            // openMeshBrowse) -- there was no way to bring a new OBJ into
            // the scene as a fresh object. Adds one, in the same current-
            // scene ("new object", not "replace scene") behavior as every
            // other Add-menu creation action.
            if (ImGui::MenuItem("Import OBJ...")) {
                importObjDialogBuf_[0] = '\0';
                importObjDialogErr_[0] = '\0';
                importObjDialogOpen_   = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save",    "Ctrl+S")) saveFile();
            if (ImGui::MenuItem("Save As...","Ctrl+Shift+S")) saveFileAs();
            ImGui::Separator();
            if (ImGui::MenuItem("Export GLB", "Ctrl+E")) exportGltf();
            // STAB-0718: previously glTF/GLB was the only export format
            // from the editor GUI. Reuses the existing GltfExporter as a
            // black box (see runObjExport()'s own comment) rather than a
            // second from-scratch scene-traversal implementation.
            if (ImGui::MenuItem("Export OBJ...")) exportObj();
            {
                bool hasSel = !selection_.selection().empty();
                if (!hasSel) ImGui::BeginDisabled();
                if (ImGui::MenuItem("Export Selection...", nullptr, false, hasSel)) {
                    selExportBuf_[0] = '\0';
                    selExportErr_[0] = '\0';
                    selExportOpen_   = true;
                }
                if (!hasSel) ImGui::EndDisabled();
            }
            if (ImGui::MenuItem("Merge Scene...")) {
                mergeSceneBuf_[0] = '\0';
                mergeSceneErr_[0] = '\0';
                mergeSceneOpen_   = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) confirmIfModified(PendingAction::ExitApp);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, undoManager_.canUndo())) performUndo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, undoManager_.canRedo())) performRedo();
            if (ImGui::MenuItem("Undo History...", nullptr, false, undoManager_.canUndo()))
                undoHistoryOpen_ = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Cut",       "Ctrl+X")) cutSelected();
            if (ImGui::MenuItem("Copy",      "Ctrl+C")) copySelected();
            if (ImGui::MenuItem("Paste",     "Ctrl+V")) pasteClipboard();
            if (ImGui::MenuItem("Duplicate",           "Ctrl+D"))       duplicateSelected();
            if (ImGui::MenuItem("Duplicate at Offset", "Ctrl+Shift+D",
                                false, selection_.hasSelection())) {
                duplicateSelected();
                for (const auto& s : selection_.selection())
                    s->transform.position[0] += gridSpacing_;
                char dbuf[64];
                std::snprintf(dbuf, sizeof(dbuf), "Duplicated %d object(s) (+%.4g u on X)",
                              static_cast<int>(selection_.selection().size()), gridSpacing_);
                setStatusMsg(dbuf);
            }
            if (ImGui::MenuItem("Delete",    "Del"))    deleteSelected();
            ImGui::Separator();
            if (ImGui::MenuItem("Select All","Ctrl+A")) {
                selection_.clear();
                for (auto& o : document_.objects) selection_.select(o);
                updateWindowTitle();
            }
            if (ImGui::MenuItem("Invert Selection", "Ctrl+I")) {
                walkAll(document_.objects, [&](const auto& o) {
                    if (selection_.isSelected(o.get())) selection_.deselect(o);
                    else                                selection_.select(o);
                });
                updateWindowTitle();
            }
            if (ImGui::BeginMenu("Select by Type")) {
                std::set<Mc3::ObjectType> presentTypes;
                walkAll(document_.objects, [&](const auto& o) { presentTypes.insert(o->type); });

                auto typeName = [](Mc3::ObjectType t) -> const char* {
                    switch (t) {
                        case Mc3::ObjectType::Box:          return "Box";
                        case Mc3::ObjectType::Cube:         return "Cube";
                        case Mc3::ObjectType::Sphere:       return "Sphere";
                        case Mc3::ObjectType::Cylinder:     return "Cylinder";
                        case Mc3::ObjectType::Cone:         return "Cone";
                        case Mc3::ObjectType::Plane:        return "Plane";
                        case Mc3::ObjectType::Mesh:         return "Mesh";
                        case Mc3::ObjectType::Extrude:      return "Extrude";
                        case Mc3::ObjectType::Group:        return "Group";
                        case Mc3::ObjectType::Instance:     return "Instance";
                        case Mc3::ObjectType::Union:        return "Union (CSG)";
                        case Mc3::ObjectType::Difference:   return "Difference (CSG)";
                        case Mc3::ObjectType::Intersection: return "Intersection (CSG)";
                        case Mc3::ObjectType::Area:         return "Area";
                        default:                            return "?";
                    }
                };

                static const Mc3::ObjectType kAllTypes[] = {
                    Mc3::ObjectType::Box, Mc3::ObjectType::Cube, Mc3::ObjectType::Sphere,
                    Mc3::ObjectType::Cylinder, Mc3::ObjectType::Cone, Mc3::ObjectType::Plane,
                    Mc3::ObjectType::Mesh, Mc3::ObjectType::Extrude, Mc3::ObjectType::Group,
                    Mc3::ObjectType::Instance, Mc3::ObjectType::Union,
                    Mc3::ObjectType::Difference, Mc3::ObjectType::Intersection,
                    Mc3::ObjectType::Area
                };
                bool anyPresent = false;
                for (auto t : kAllTypes) {
                    if (!presentTypes.count(t)) continue;
                    anyPresent = true;
                    if (ImGui::MenuItem(typeName(t))) {
                        selectBy([t](const auto& o) { return o->type == t; });
                    }
                }
                if (!anyPresent) ImGui::TextDisabled("(scene is empty)");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Select by Tag")) {
                std::set<std::string> allTags;
                walkAll(document_.objects, [&](const auto& o) {
                    for (const auto& t : o->tags) allTags.insert(t);
                });

                if (allTags.empty()) {
                    ImGui::TextDisabled("(no tags in scene)");
                } else {
                    for (const auto& tag : allTags) {
                        if (ImGui::MenuItem(tag.c_str())) {
                            selectBy([&](const auto& o) {
                                return std::find(o->tags.begin(), o->tags.end(), tag) != o->tags.end();
                            });
                            char sbuf[96];
                            std::snprintf(sbuf, sizeof(sbuf), "Selected %d object(s) with tag \"%s\"",
                                          static_cast<int>(selection_.selection().size()), tag.c_str());
                            setStatusMsg(sbuf);
                        }
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Select by Material")) {
                std::set<std::string> allMats;
                walkAll(document_.objects, [&](const auto& o) {
                    if (!o->material.empty()) allMats.insert(o->material);
                });
                if (allMats.empty()) {
                    ImGui::TextDisabled("(no materials in scene)");
                } else {
                    for (const auto& mat : allMats) {
                        if (ImGui::MenuItem(mat.c_str())) {
                            selectBy([&](const auto& o) { return o->material == mat; });
                            char sbuf[96];
                            std::snprintf(sbuf, sizeof(sbuf), "Selected %d object(s) with material \"%s\"",
                                          static_cast<int>(selection_.selection().size()), mat.c_str());
                            setStatusMsg(sbuf);
                        }
                    }
                }
                ImGui::EndMenu();
            }
            {
                bool hasSel2plus = selection_.selection().size() >= 2;
                if (ImGui::MenuItem("Copy Properties to Selected...", "Ctrl+Shift+P", false, hasSel2plus))
                    copyPropsOpen_ = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Group",   "Ctrl+G"))       groupSelected();
            if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G")) ungroupSelected();
            if (ImGui::MenuItem("Convert to Definition", nullptr, false, !selection_.selection().empty()))
                convertToDefinition();
            if (ImGui::MenuItem("Export Subtree as Template...", nullptr, false, !selection_.selection().empty())) {
                auto* src = selection_.selection().front().get();
                std::string suggested = src->name.empty() ? src->id : src->name;
                std::strncpy(subtreeExportNameBuf_, suggested.c_str(), sizeof(subtreeExportNameBuf_)-1);
                subtreeExportNameBuf_[sizeof(subtreeExportNameBuf_)-1] = '\0';
                subtreeExportFileBuf_[0] = '\0';
                subtreeExportErr_[0]     = '\0';
                subtreeExportOpen_       = true;
            }
            {
                bool isInst = !selection_.selection().empty() &&
                              selection_.selection().front()->type == Mc3::ObjectType::Instance;
                if (ImGui::MenuItem("Break Instance", nullptr, false, isInst))
                    breakInstance();
            }
            bool hasSel2 = !selection_.selection().empty();
            if (ImGui::BeginMenu("Align Selection", hasSel2)) {
                // Compute bounding box of selected objects' pivot positions
                float minV[3] = {1e30f, 1e30f, 1e30f};
                float maxV[3] = {-1e30f,-1e30f,-1e30f};
                for (const auto& s : selection_.selection()) {
                    for (int i = 0; i < 3; ++i) {
                        float v = s->transform.position[i];
                        minV[i] = std::min(minV[i], v);
                        maxV[i] = std::max(maxV[i], v);
                    }
                }
                float cenV[3] = {
                    (minV[0]+maxV[0])*0.5f,
                    (minV[1]+maxV[1])*0.5f,
                    (minV[2]+maxV[2])*0.5f
                };

                auto doAlign = [&](int axis, float target) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (lockedIds_.count(s->id)) continue;
                        s->transform.position[axis] = target;
                    }
                    modified_ = true; updateWindowTitle();
                };

                static const char* minLabel[3] = {"Min X (left)",  "Min Y (bottom)", "Min Z (front)"};
                static const char* cenLabel[3] = {"Center X",      "Center Y",       "Center Z"     };
                static const char* maxLabel[3] = {"Max X (right)", "Max Y (top)",    "Max Z (back)" };

                for (int ax = 0; ax < 3; ++ax) {
                    if (ax > 0) ImGui::Separator();
                    if (ImGui::MenuItem(minLabel[ax])) doAlign(ax, minV[ax]);
                    if (ImGui::MenuItem(cenLabel[ax])) doAlign(ax, cenV[ax]);
                    if (ImGui::MenuItem(maxLabel[ax])) doAlign(ax, maxV[ax]);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("To First Selected (XYZ)",
                                    nullptr, false,
                                    selection_.selection().size() >= 2)) {
                    alignToObject();
                }
                ImGui::EndMenu();
            }
            bool hasSel2b = selection_.selection().size() >= 2;
            if (ImGui::BeginMenu("Distribute Selection", hasSel2b)) {
                static const char* distLabel[3] = {
                    "Distribute X (spacing)", "Distribute Y (spacing)", "Distribute Z (spacing)"
                };
                for (int ax = 0; ax < 3; ++ax) {
                    if (ImGui::MenuItem(distLabel[ax])) {
                        // Collect & sort by position on this axis
                        auto objs = selection_.selection();
                        std::sort(objs.begin(), objs.end(),
                            [ax](const auto& a, const auto& b) {
                                return a->transform.position[ax] < b->transform.position[ax];
                            });
                        int n = static_cast<int>(objs.size());
                        if (n >= 2) {
                            pushUndo();
                            float lo = objs.front()->transform.position[ax];
                            float hi = objs.back()->transform.position[ax];
                            for (int i = 1; i < n - 1; ++i) {
                                if (lockedIds_.count(objs[i]->id)) continue;
                                objs[i]->transform.position[ax] =
                                    lo + static_cast<float>(i) * (hi - lo) / static_cast<float>(n - 1);
                            }
                            modified_ = true; updateWindowTitle();
                        }
                    }
                }
                ImGui::EndMenu();
            }
            {
                bool hasSel2c = !selection_.selection().empty();
                if (ImGui::MenuItem("Drop to Ground Plane", nullptr, false, hasSel2c)) {
                    dropSelectedToGroundPlane();
                }
                if (ImGui::MenuItem("Snap Selection to Grid", nullptr, false, hasSel2c)) {
                    int snapped = 0;
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (lockedIds_.count(s->id)) continue;
                        for (int i = 0; i < 3; ++i)
                            s->transform.position[i] =
                                std::round(s->transform.position[i] / gridSpacing_) * gridSpacing_;
                        ++snapped;
                    }
                    modified_ = true; updateWindowTitle();
                    char gsbuf[64];
                    std::snprintf(gsbuf, sizeof(gsbuf), "Snapped %d object(s) to grid (%.4g u)", snapped, gridSpacing_);
                    setStatusMsg(gsbuf);
                }

                // Mirror / flip
                bool hasSel2d = !selection_.selection().empty();
                if (ImGui::BeginMenu("Mirror Selection", hasSel2d)) {
                    static const char* flipLabel[3] = { "Flip X", "Flip Y", "Flip Z" };
                    for (int ax = 0; ax < 3; ++ax) {
                        if (ImGui::MenuItem(flipLabel[ax])) {
                            pushUndo();
                            for (const auto& s : selection_.selection()) {
                                if (lockedIds_.count(s->id)) continue;
                                s->transform.scale[ax] = -s->transform.scale[ax];
                            }
                            modified_ = true; updateWindowTitle();
                            char mbuf[48];
                            std::snprintf(mbuf, sizeof(mbuf), "Mirrored %d object(s) on %c",
                                          static_cast<int>(selection_.selection().size()),
                                          "XYZ"[ax]);
                            setStatusMsg(mbuf);
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            if (ImGui::MenuItem("Group Scale…", nullptr, false,
                                selection_.selection().size() >= 2))
                groupScaleOpen_ = true;
            if (ImGui::MenuItem("Linear Array...", nullptr, false,
                                !selection_.selection().empty()))
                arrayDupOpen_ = true;
            if (ImGui::MenuItem("Scatter Along Curve...", nullptr, false,
                                !selection_.selection().empty()))
                scatterCurveOpen_ = true;
            if (ImGui::MenuItem("Batch Rename...", "Ctrl+Shift+R", false,
                                !selection_.selection().empty()))
                batchRenameOpen_ = true;
            if (ImGui::MenuItem("Find & Replace Names...", "Ctrl+H"))
                findReplaceOpen_ = true;
            if (ImGui::MenuItem("Randomize Transform...", nullptr, false,
                                !selection_.selection().empty()))
                randomizeOpen_ = true;
            ImGui::Separator();
            // H12: Macro recorder (SYS-W3-01 Phase 3: Editor::MacroRecorder)
            if (macroRecorder_.isRecording()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.f));
                if (ImGui::MenuItem("Stop Recording")) {
                    macroRecorder_.stopRecording();
                    setStatusMsg("Recording stopped", false, 2.f);
                }
                ImGui::PopStyleColor();
            } else {
                if (ImGui::MenuItem("Record Macro")) {
                    macroRecorder_.startRecording();
                    setStatusMsg("Recording started — edit the scene, then Stop", false, 3.f);
                }
            }
            if (ImGui::MenuItem("Play Macro", nullptr, false,
                                 macroRecorder_.stepCount() > 0 && !macroRecorder_.isRecording()))
                macroRecorder_.play(macroContext());
            if (ImGui::MenuItem("Macro Editor…"))
                macroOpen_ = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Lock/Unlock Selected", "Ctrl+L", false, !selection_.selection().empty())) {
                for (const auto& s : selection_.selection()) {
                    if (lockedIds_.count(s->id)) lockedIds_.erase(s->id);
                    else                          lockedIds_.insert(s->id);
                }
            }
            ImGui::Separator();
            bool hasSel = !selection_.selection().empty();
            if (ImGui::BeginMenu("Reset Transform", hasSel)) {
                if (ImGui::MenuItem("Position", "Alt+G")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (lockedIds_.count(s->id)) continue;
                        s->transform.position = {0.0f, 0.0f, 0.0f};
                    }
                    modified_ = true; updateWindowTitle();
                }
                if (ImGui::MenuItem("Rotation", "Alt+R")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (lockedIds_.count(s->id)) continue;
                        s->transform.rotation = {0.0f, 0.0f, 0.0f};
                    }
                    modified_ = true; updateWindowTitle();
                }
                if (ImGui::MenuItem("Scale", "Alt+S")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (lockedIds_.count(s->id)) continue;
                        s->transform.scale = {1.0f, 1.0f, 1.0f};
                    }
                    modified_ = true; updateWindowTitle();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("All")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (lockedIds_.count(s->id)) continue;
                        s->transform.position = {0.0f, 0.0f, 0.0f};
                        s->transform.rotation = {0.0f, 0.0f, 0.0f};
                        s->transform.scale    = {1.0f, 1.0f, 1.0f};
                    }
                    modified_ = true; updateWindowTitle();
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy Transform", "Ctrl+Shift+C", false, hasSel)) {
                const auto& src = selection_.selection().front();
                transformClipboard_.copyFrom(*src);
                setStatusMsg("Transform copied from \"" + src->name + "\"");
            }
            if (ImGui::MenuItem("Paste Transform", "Ctrl+Shift+V", false, hasSel && transformClipboard_.hasValue())) {
                pushUndo();
                for (const auto& s : selection_.selection()) {
                    if (lockedIds_.count(s->id)) continue;
                    transformClipboard_.pasteTo(*s);
                }
                modified_ = true; updateWindowTitle();
                setStatusMsg("Transform pasted to " + std::to_string(selection_.selection().size()) + " object(s)");
            }
            ImGui::Separator();
            if (ImGui::MenuItem(isolateActive_ ? "Exit Isolation" : "Isolate Selection",
                               "Alt+I", false,
                               isolateActive_ || !selection_.selection().empty())) {
                toggleIsolate();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Hide Selected", "H", false, !selection_.selection().empty())) {
                auto sel = selection_.selection();
                pushUndo();
                for (auto& s : sel) s->visible = false;
                selection_.clear();
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::MenuItem("Show All Hidden", "Alt+H")) {
                pushUndo();
                walkAll(document_.objects, [&](const auto& o) { o->visible = true; });
                modified_ = true; updateWindowTitle();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Box",      "F1")) addPrimitive(Mc3::ObjectType::Box);
            if (ImGui::MenuItem("Sphere",   "F2")) addPrimitive(Mc3::ObjectType::Sphere);
            if (ImGui::MenuItem("Cylinder", "F3")) addPrimitive(Mc3::ObjectType::Cylinder);
            if (ImGui::MenuItem("Cone",     "F4")) addPrimitive(Mc3::ObjectType::Cone);
            if (ImGui::MenuItem("Plane",    "F5")) addPrimitive(Mc3::ObjectType::Plane);
            ImGui::Separator();
            if (ImGui::MenuItem("Area"))     addPrimitive(Mc3::ObjectType::Area);
            if (ImGui::MenuItem("Mesh"))     addPrimitive(Mc3::ObjectType::Mesh);
            if (ImGui::MenuItem("Instance")) addPrimitive(Mc3::ObjectType::Instance);
            if (ImGui::MenuItem("Extrude"))  addPrimitive(Mc3::ObjectType::Extrude);
            if (ImGui::MenuItem("Group"))    addPrimitive(Mc3::ObjectType::Group);
            ImGui::Separator();
            if (ImGui::BeginMenu("CSG")) {
                if (ImGui::MenuItem("Union"))        addPrimitive(Mc3::ObjectType::Union);
                if (ImGui::MenuItem("Difference"))   addPrimitive(Mc3::ObjectType::Difference);
                if (ImGui::MenuItem("Intersection")) addPrimitive(Mc3::ObjectType::Intersection);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Front",  "Num1")) { camera_.yaw = 0.0f;                               camera_.pitch = 0.0f; }
            if (ImGui::MenuItem("Right",  "Num3")) { camera_.yaw = std::numbers::pi_v<float> * 0.5f;  camera_.pitch = 0.0f; }
            if (ImGui::MenuItem("Back",   "Num5")) { camera_.yaw = std::numbers::pi_v<float>;          camera_.pitch = 0.0f; }
            if (ImGui::MenuItem("Top",    "Num7")) { camera_.yaw = 0.0f;                               camera_.pitch = 1.47f; }
            if (ImGui::MenuItem("Bottom", "Num9")) { camera_.yaw = 0.0f;                               camera_.pitch =-1.47f; }
            ImGui::Separator();
            if (ImGui::MenuItem("Focus on selection", "F")) {
                if (selection_.hasSelection()) {
                    auto* s = selection_.selection().front().get();
                    camera_.focusOn(s->transform.position[0], s->transform.position[1], s->transform.position[2]);
                } else { camera_.reset(); }
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Camera Bookmarks")) {
                static const char* kSlotKeys[5] = {"Ctrl+F1","Ctrl+F2","Ctrl+F3","Ctrl+F4","Ctrl+F5"};
                static const char* kRestKeys[5] = {"F6","F7","F8","F9","F10"};
                for (int i = 0; i < 5; ++i) {
                    const auto* bm = cameraBookmarks_.get(i);
                    char saveLabel[48];
                    std::snprintf(saveLabel, sizeof(saveLabel), "Save Slot %d", i + 1);
                    if (ImGui::MenuItem(saveLabel, kSlotKeys[i]))
                        saveCameraBookmark(i);
                    char restLabel[80];
                    if (bm && bm->valid)
                        std::snprintf(restLabel, sizeof(restLabel),
                            "Go to Slot %d  [%.1f,%.1f,%.1f]", i+1, bm->targetX, bm->targetY, bm->targetZ);
                    else
                        std::snprintf(restLabel, sizeof(restLabel), "Go to Slot %d  (empty)", i+1);
                    if (ImGui::MenuItem(restLabel, kRestKeys[i], false, bm && bm->valid))
                        restoreCameraBookmark(i);
                    if (i < 4) ImGui::Separator();
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Walk Mode", "F5", walkController_.isActive())) {
                if (walkController_.isActive()) exitWalkMode(); else enterWalkMode();
            }
            ImGui::Separator();
            ImGui::MenuItem("Edge Overlay",    "Alt+W", &showEdgeOverlay_);
            ImGui::MenuItem("Wireframe Mode",  nullptr,  &showWireframeMode_);
            ImGui::MenuItem("Stats Overlay",   nullptr,  &showStatsOverlay_);
            ImGui::MenuItem("Bloom (emissive glow)", nullptr, &bloomEnabled_);
            if (bloomEnabled_) {
                ImGui::SetNextItemWidth(140);
                // AlwaysClamp: without it, Ctrl+Click lets a typed value go
                // out of [min,max] (incl. negative), which the composite
                // shader has no other guard against (STAB-0325).
                ImGui::SliderFloat("  Strength##bloom", &bloomStrength_, 0.5f, 8.0f, "%.1f",
                                   ImGuiSliderFlags_AlwaysClamp);
            }
            ImGui::MenuItem("SSAO (ambient occlusion)", nullptr, &ssaoEnabled_);
            if (ssaoEnabled_) {
                ImGui::SetNextItemWidth(140);
                // AlwaysClamp: same out-of-bounds-via-Ctrl+Click risk as
                // bloom strength above (STAB-0326).
                ImGui::SliderFloat("  Strength##ssao", &ssaoStrength_, 0.0f, 1.0f, "%.2f",
                                   ImGuiSliderFlags_AlwaysClamp);
                ImGui::SetNextItemWidth(140);
                // Same fix applied here too: a negative/zero radius from an
                // unclamped Ctrl+Click entry would break the SSAO sample
                // kernel, same root cause as the two strength sliders above.
                ImGui::SliderFloat("  Radius##ssao",   &ssaoRadius_,   0.05f, 2.0f, "%.2f",
                                   ImGuiSliderFlags_AlwaysClamp);
            }
            ImGui::MenuItem("Shadow Map Debug",         nullptr, &shadowDebugEnabled_);
            ImGui::MenuItem("Snap to Grid", nullptr, &snapEnabled_);
            ImGui::MenuItem("Timeline",       "Ctrl+T", &showTimeline_);
            ImGui::MenuItem("Model Registry", nullptr,  &showRegistryPanel_);
            ImGui::MenuItem("AI Assistant",   nullptr,  &showAiPanel_);
            ImGui::MenuItem("Validation",     nullptr,  &showValidationPanel_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Preferences...")) prefs_.setWindowOpen(true);
            ImGui::Separator();
            if (ImGui::MenuItem("Command Palette...", "Ctrl+P")) cmdPaletteOpen_ = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Keyboard Shortcuts...")) showShortcutsDialog_ = true;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    return menuBarH;
}


} // namespace MeshCraft
