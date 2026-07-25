#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include "MeshCraft/Application/UI/MenuBar.hpp"
#include "MeshCraft/GraphicsBackendCheck.hpp"
#include "MeshCraft/MeshCraftPrivate.hpp"

#include <imgui.h>

#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/Viewport.hpp>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <set>
#include <string>

namespace MeshCraft::Application {

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
            const UI::EditHistoryContext editHistoryContext{
                .canUndo = undoManager_.canUndo(),
                .canRedo = undoManager_.canRedo(),
                .undo = [this] { performUndo(); },
                .redo = [this] { performRedo(); },
                .openHistory = [this] { undoHistoryOpen_ = true; },
            };
            UI::MenuBar::drawEditHistory(editHistoryContext);
            const UI::EditClipboardContext editClipboardContext{
                .cut = [this] { cutSelected(); },
                .copy = [this] { copySelected(); },
                .paste = [this] { pasteClipboard(); },
            };
            UI::MenuBar::drawEditClipboard(editClipboardContext);
            const UI::EditObjectActionsContext editObjectActionsContext{
                .canDuplicateAtOffset = selection_.hasSelection(),
                .duplicate = [this] { duplicateSelected(); },
                .duplicateAtOffset = [this] {
                    duplicateSelected();
                    for (const auto& s : selection_.selection())
                        s->transform.position[0] += gridSpacing_;
                    char dbuf[64];
                    std::snprintf(dbuf, sizeof(dbuf), "Duplicated %d object(s) (+%.4g u on X)",
                                  static_cast<int>(selection_.selection().size()), gridSpacing_);
                    setStatusMsg(dbuf);
                },
                .deleteSelection = [this] { deleteSelected(); },
            };
            UI::MenuBar::drawEditObjectActions(editObjectActionsContext);
            const UI::EditSelectionActionsContext editSelectionActionsContext{
                .selectAll = [this] {
                    selection_.clear();
                    for (auto& o : document_.objects) selection_.select(o);
                    updateWindowTitle();
                },
                .invertSelection = [this, &walkAll] {
                    walkAll(document_.objects, [&](const auto& o) {
                        if (selection_.isSelected(o.get())) selection_.deselect(o);
                        else                                selection_.select(o);
                    });
                    updateWindowTitle();
                },
            };
            UI::MenuBar::drawEditSelectionActions(editSelectionActionsContext);
            const UI::EditSelectByTypeContext editSelectByTypeContext{
                .getPresentTypes = [this, &walkAll] {
                    std::vector<Mc3::ObjectType> presentTypes;
                    presentTypes.reserve(static_cast<std::size_t>(Mc3::ObjectType::Area) + 1);
                    walkAll(document_.objects, [&](const auto& o) {
                        if (std::find(presentTypes.begin(), presentTypes.end(), o->type) ==
                            presentTypes.end()) {
                            presentTypes.push_back(o->type);
                        }
                    });
                    return presentTypes;
                },
                .selectType = [&selectBy](Mc3::ObjectType type) {
                    selectBy([type](const auto& o) { return o->type == type; });
                },
            };
            UI::MenuBar::drawEditSelectByType(editSelectByTypeContext);
            const UI::EditSelectByTagContext editSelectByTagContext{
                .getTags = [this, &walkAll] {
                    std::set<std::string> allTags;
                    walkAll(document_.objects, [&](const auto& o) {
                        for (const auto& tag : o->tags) allTags.insert(tag);
                    });
                    return allTags;
                },
                .selectTag = [this, &selectBy](const std::string& tag) {
                    selectBy([&tag](const auto& o) {
                        return std::find(o->tags.begin(), o->tags.end(), tag) != o->tags.end();
                    });
                    char sbuf[96];
                    std::snprintf(sbuf, sizeof(sbuf), "Selected %d object(s) with tag \"%s\"",
                                  static_cast<int>(selection_.selection().size()), tag.c_str());
                    setStatusMsg(sbuf);
                },
            };
            UI::MenuBar::drawEditSelectByTag(editSelectByTagContext);
            const UI::EditSelectByMaterialContext editSelectByMaterialContext{
                .getMaterials = [this, &walkAll] {
                    std::set<std::string> allMaterials;
                    walkAll(document_.objects, [&](const auto& o) {
                        if (!o->material.empty()) allMaterials.insert(o->material);
                    });
                    return allMaterials;
                },
                .selectMaterial = [this, &selectBy](const std::string& material) {
                    selectBy([&material](const auto& o) { return o->material == material; });
                    char sbuf[96];
                    std::snprintf(sbuf, sizeof(sbuf), "Selected %d object(s) with material \"%s\"",
                                  static_cast<int>(selection_.selection().size()), material.c_str());
                    setStatusMsg(sbuf);
                },
            };
            UI::MenuBar::drawEditSelectByMaterial(editSelectByMaterialContext);
            const UI::EditCopyPropertiesContext editCopyPropertiesContext{
                .canCopy = selection_.selection().size() >= 2,
                .openDialog = [this] { copyPropsOpen_ = true; },
            };
            UI::MenuBar::drawEditCopyProperties(editCopyPropertiesContext);
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
                        if (objectLockState_.isLocked(s->id)) continue;
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
                                if (objectLockState_.isLocked(objs[i]->id)) continue;
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
                        if (objectLockState_.isLocked(s->id)) continue;
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
                                if (objectLockState_.isLocked(s->id)) continue;
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
                    objectLockState_.toggle(s->id);
                }
            }
            ImGui::Separator();
            bool hasSel = !selection_.selection().empty();
            if (ImGui::BeginMenu("Reset Transform", hasSel)) {
                if (ImGui::MenuItem("Position", "Alt+G")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (objectLockState_.isLocked(s->id)) continue;
                        s->transform.position = {0.0f, 0.0f, 0.0f};
                    }
                    modified_ = true; updateWindowTitle();
                }
                if (ImGui::MenuItem("Rotation", "Alt+R")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (objectLockState_.isLocked(s->id)) continue;
                        s->transform.rotation = {0.0f, 0.0f, 0.0f};
                    }
                    modified_ = true; updateWindowTitle();
                }
                if (ImGui::MenuItem("Scale", "Alt+S")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (objectLockState_.isLocked(s->id)) continue;
                        s->transform.scale = {1.0f, 1.0f, 1.0f};
                    }
                    modified_ = true; updateWindowTitle();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("All")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (objectLockState_.isLocked(s->id)) continue;
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
                    if (objectLockState_.isLocked(s->id)) continue;
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
        UI::MenuBar::drawAddMenu(
            [this](Mc3::ObjectType type) { addPrimitive(type); });
        if (ImGui::BeginMenu("View")) {
            UI::MenuBar::drawViewDirections(camera_.yaw, camera_.pitch);
            ImGui::Separator();
            UI::MenuBar::drawFocusSelection([this] {
                if (selection_.hasSelection()) {
                    auto* s = selection_.selection().front().get();
                    camera_.focusOn(s->transform.position[0], s->transform.position[1], s->transform.position[2]);
                } else { camera_.reset(); }
            });
            ImGui::Separator();
            const UI::CameraBookmarksContext cameraBookmarksContext{
                .bookmarks = cameraBookmarks_,
                .save = [this](int slot) { saveCameraBookmark(slot); },
                .restore = [this](int slot) { restoreCameraBookmark(slot); },
            };
            UI::MenuBar::drawCameraBookmarks(cameraBookmarksContext);
            ImGui::Separator();
            const UI::WalkModeContext walkModeContext{
                .active = walkController_.isActive(),
                .toggle = [this] {
                    if (walkController_.isActive()) exitWalkMode(); else enterWalkMode();
                },
            };
            UI::MenuBar::drawWalkMode(walkModeContext);
            ImGui::Separator();
            UI::MenuBar::drawOverlays(showEdgeOverlay_, showWireframeMode_,
                                      showStatsOverlay_, shadowDebugEnabled_, snapEnabled_);
            if (supportsTextShaderEffects()) {
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
            } else {
                ImGui::TextDisabled("Bloom and SSAO require cross-backend ShaderEffect support");
            }
            UI::MenuBar::drawPanelToggles(showTimeline_, showRegistryPanel_,
                                          showAiPanel_, showValidationPanel_);
            ImGui::EndMenu();
        }
        const UI::HelpMenuContext helpMenuContext{
            .openPreferences = [this] { prefs_.setWindowOpen(true); },
            .openCommandPalette = [this] { cmdPaletteOpen_ = true; },
            .openKeyboardShortcuts = [this] { showShortcutsDialog_ = true; },
        };
        UI::MenuBar::drawHelpMenu(helpMenuContext);
        ImGui::EndMainMenuBar();
    }
    return menuBarH;
}


} // namespace MeshCraft::Application

namespace MeshCraft::Application::UI {

void MenuBar::drawAddMenu(const std::function<void(Mc3::ObjectType)>& addPrimitive) {
    if (!ImGui::BeginMenu("Add")) return;

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

void MenuBar::drawEditHistory(const EditHistoryContext& context) {
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, context.canUndo)) context.undo();
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, context.canRedo)) context.redo();
    if (ImGui::MenuItem("Undo History...", nullptr, false, context.canUndo)) {
        context.openHistory();
    }
    ImGui::Separator();
}

void MenuBar::drawEditClipboard(const EditClipboardContext& context) {
    if (ImGui::MenuItem("Cut",  "Ctrl+X")) context.cut();
    if (ImGui::MenuItem("Copy", "Ctrl+C")) context.copy();
    if (ImGui::MenuItem("Paste", "Ctrl+V")) context.paste();
}

void MenuBar::drawEditObjectActions(const EditObjectActionsContext& context) {
    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) context.duplicate();
    if (ImGui::MenuItem("Duplicate at Offset", "Ctrl+Shift+D", false,
                        context.canDuplicateAtOffset)) {
        context.duplicateAtOffset();
    }
    if (ImGui::MenuItem("Delete", "Del")) context.deleteSelection();
    ImGui::Separator();
}

void MenuBar::drawEditSelectionActions(const EditSelectionActionsContext& context) {
    if (ImGui::MenuItem("Select All", "Ctrl+A")) context.selectAll();
    if (ImGui::MenuItem("Invert Selection", "Ctrl+I")) context.invertSelection();
}

void MenuBar::drawEditSelectByType(const EditSelectByTypeContext& context) {
    if (!ImGui::BeginMenu("Select by Type")) return;

    const auto presentTypes = context.getPresentTypes();
    auto typeName = [](Mc3::ObjectType type) -> const char* {
        switch (type) {
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
    for (auto type : kAllTypes) {
        if (std::find(presentTypes.begin(), presentTypes.end(), type) == presentTypes.end()) {
            continue;
        }
        anyPresent = true;
        if (ImGui::MenuItem(typeName(type))) context.selectType(type);
    }
    if (!anyPresent) ImGui::TextDisabled("(scene is empty)");
    ImGui::EndMenu();
}

void MenuBar::drawEditSelectByTag(const EditSelectByTagContext& context) {
    if (!ImGui::BeginMenu("Select by Tag")) return;

    const auto tags = context.getTags();
    if (tags.empty()) {
        ImGui::TextDisabled("(no tags in scene)");
    } else {
        for (const auto& tag : tags) {
            if (ImGui::MenuItem(tag.c_str())) context.selectTag(tag);
        }
    }
    ImGui::EndMenu();
}

void MenuBar::drawEditSelectByMaterial(const EditSelectByMaterialContext& context) {
    if (!ImGui::BeginMenu("Select by Material")) return;

    const auto materials = context.getMaterials();
    if (materials.empty()) {
        ImGui::TextDisabled("(no materials in scene)");
    } else {
        for (const auto& material : materials) {
            if (ImGui::MenuItem(material.c_str())) context.selectMaterial(material);
        }
    }
    ImGui::EndMenu();
}

void MenuBar::drawEditCopyProperties(const EditCopyPropertiesContext& context) {
    if (ImGui::MenuItem("Copy Properties to Selected...", "Ctrl+Shift+P", false,
                        context.canCopy)) {
        context.openDialog();
    }
    ImGui::Separator();
}

void MenuBar::drawPanelToggles(bool& timeline, bool& registry, bool& ai, bool& validation) {
    ImGui::MenuItem("Timeline", "Ctrl+T", &timeline);
    ImGui::MenuItem("Model Registry", nullptr, &registry);
    ImGui::MenuItem("AI Assistant", nullptr, &ai);
    ImGui::MenuItem("Validation", nullptr, &validation);
}

void MenuBar::drawOverlays(bool& edges, bool& wireframe, bool& stats, bool& shadowDebug, bool& snap) {
    ImGui::MenuItem("Edge Overlay", "Alt+W", &edges);
    ImGui::MenuItem("Wireframe Mode", nullptr, &wireframe);
    ImGui::MenuItem("Stats Overlay", nullptr, &stats);
    ImGui::MenuItem("Shadow Map Debug", nullptr, &shadowDebug);
    ImGui::MenuItem("Snap to Grid", nullptr, &snap);
}

void MenuBar::drawViewDirections(float& yaw, float& pitch) {
    if (ImGui::MenuItem("Front", "Num1")) { yaw = 0.0f; pitch = 0.0f; }
    if (ImGui::MenuItem("Right", "Num3")) { yaw = std::numbers::pi_v<float> * 0.5f; pitch = 0.0f; }
    if (ImGui::MenuItem("Back", "Num5")) { yaw = std::numbers::pi_v<float>; pitch = 0.0f; }
    if (ImGui::MenuItem("Top", "Num7")) { yaw = 0.0f; pitch = 1.47f; }
    if (ImGui::MenuItem("Bottom", "Num9")) { yaw = 0.0f; pitch = -1.47f; }
}

void MenuBar::drawFocusSelection(const std::function<void()>& focus) {
    if (ImGui::MenuItem("Focus on selection", "F")) focus();
}

void MenuBar::drawCameraBookmarks(const CameraBookmarksContext& context) {
    if (!ImGui::BeginMenu("Camera Bookmarks")) return;

    static const char* kSlotKeys[Editor::CameraBookmarks::kSlotCount] = {
        "Ctrl+F1", "Ctrl+F2", "Ctrl+F3", "Ctrl+F4", "Ctrl+F5"
    };
    static const char* kRestoreKeys[Editor::CameraBookmarks::kSlotCount] = {
        "F6", "F7", "F8", "F9", "F10"
    };
    for (int slot = 0; slot < Editor::CameraBookmarks::kSlotCount; ++slot) {
        const auto* bookmark = context.bookmarks.get(slot);
        char saveLabel[48];
        std::snprintf(saveLabel, sizeof(saveLabel), "Save Slot %d", slot + 1);
        if (ImGui::MenuItem(saveLabel, kSlotKeys[slot])) context.save(slot);

        char restoreLabel[80];
        if (bookmark && bookmark->valid) {
            std::snprintf(restoreLabel, sizeof(restoreLabel),
                          "Go to Slot %d  [%.1f,%.1f,%.1f]", slot + 1,
                          bookmark->targetX, bookmark->targetY, bookmark->targetZ);
        } else {
            std::snprintf(restoreLabel, sizeof(restoreLabel),
                          "Go to Slot %d  (empty)", slot + 1);
        }
        if (ImGui::MenuItem(restoreLabel, kRestoreKeys[slot], false,
                            bookmark && bookmark->valid)) {
            context.restore(slot);
        }
        if (slot + 1 < Editor::CameraBookmarks::kSlotCount) ImGui::Separator();
    }
    ImGui::EndMenu();
}

void MenuBar::drawWalkMode(const WalkModeContext& context) {
    if (ImGui::MenuItem("Walk Mode", "F5", context.active)) context.toggle();
}

void MenuBar::drawHelpMenu(const HelpMenuContext& context) {
    if (!ImGui::BeginMenu("Help")) return;

    if (ImGui::MenuItem("Preferences...")) context.openPreferences();
    ImGui::Separator();
    if (ImGui::MenuItem("Command Palette...", "Ctrl+P")) context.openCommandPalette();
    ImGui::Separator();
    if (ImGui::MenuItem("Keyboard Shortcuts...")) context.openKeyboardShortcuts();

    ImGui::EndMenu();
}

} // namespace MeshCraft::Application::UI
