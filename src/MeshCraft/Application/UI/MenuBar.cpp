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
            const UI::FileMergeSceneContext fileMergeSceneContext{
                .openDialog = [this] {
                    mergeSceneBuf_[0] = '\0';
                    mergeSceneErr_[0] = '\0';
                    mergeSceneOpen_   = true;
                },
            };
            UI::MenuBar::drawFileMergeScene(fileMergeSceneContext);
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
            const UI::EditGroupingContext editGroupingContext{
                .group = [this] { groupSelected(); },
                .ungroup = [this] { ungroupSelected(); },
            };
            UI::MenuBar::drawEditGrouping(editGroupingContext);
            const UI::EditConvertToDefinitionContext editConvertToDefinitionContext{
                .canConvert = !selection_.selection().empty(),
                .convert = [this] { convertToDefinition(); },
            };
            UI::MenuBar::drawEditConvertToDefinition(editConvertToDefinitionContext);
            const UI::EditExportSubtreeContext editExportSubtreeContext{
                .canExport = !selection_.selection().empty(),
                .openDialog = [this] {
                    auto* src = selection_.selection().front().get();
                    std::string suggested = src->name.empty() ? src->id : src->name;
                    std::strncpy(subtreeExportNameBuf_, suggested.c_str(),
                                 sizeof(subtreeExportNameBuf_) - 1);
                    subtreeExportNameBuf_[sizeof(subtreeExportNameBuf_) - 1] = '\0';
                    subtreeExportFileBuf_[0] = '\0';
                    subtreeExportErr_[0] = '\0';
                    subtreeExportOpen_ = true;
                },
            };
            UI::MenuBar::drawEditExportSubtree(editExportSubtreeContext);
            const UI::EditBreakInstanceContext editBreakInstanceContext{
                .canBreak = !selection_.selection().empty() &&
                            selection_.selection().front()->type == Mc3::ObjectType::Instance,
                .breakInstance = [this] { breakInstance(); },
            };
            UI::MenuBar::drawEditBreakInstance(editBreakInstanceContext);
            const UI::EditAlignSelectionContext editAlignSelectionContext{
                .canAlign = !selection_.selection().empty(),
                .canAlignToFirst = selection_.selection().size() >= 2,
                .align = [this](int axis, UI::EditAlignmentTarget target) {
                    float minPosition = 1e30f;
                    float maxPosition = -1e30f;
                    for (const auto& selected : selection_.selection()) {
                        const float position = selected->transform.position[axis];
                        minPosition = std::min(minPosition, position);
                        maxPosition = std::max(maxPosition, position);
                    }

                    float targetPosition = minPosition;
                    if (target == UI::EditAlignmentTarget::Center) {
                        targetPosition = (minPosition + maxPosition) * 0.5f;
                    } else if (target == UI::EditAlignmentTarget::Maximum) {
                        targetPosition = maxPosition;
                    }

                    pushUndo();
                    for (const auto& selected : selection_.selection()) {
                        if (objectLockState_.isLocked(selected->id)) continue;
                        selected->transform.position[axis] = targetPosition;
                    }
                    modified_ = true;
                    updateWindowTitle();
                },
                .alignToFirst = [this] { alignToObject(); },
            };
            UI::MenuBar::drawEditAlignSelection(editAlignSelectionContext);
            const UI::EditDistributeSelectionContext editDistributeSelectionContext{
                .canDistribute = selection_.selection().size() >= 2,
                .distribute = [this](int axis) {
                    auto objects = selection_.selection();
                    std::sort(objects.begin(), objects.end(),
                              [axis](const auto& left, const auto& right) {
                                  return left->transform.position[axis] <
                                         right->transform.position[axis];
                              });
                    const int count = static_cast<int>(objects.size());
                    if (count < 2) return;

                    pushUndo();
                    const float minimum = objects.front()->transform.position[axis];
                    const float maximum = objects.back()->transform.position[axis];
                    for (int index = 1; index < count - 1; ++index) {
                        if (objectLockState_.isLocked(objects[index]->id)) continue;
                        objects[index]->transform.position[axis] =
                            minimum + (static_cast<float>(index) * (maximum - minimum) /
                                       static_cast<float>(count - 1));
                    }
                    modified_ = true;
                    updateWindowTitle();
                },
            };
            UI::MenuBar::drawEditDistributeSelection(editDistributeSelectionContext);
            const UI::EditDropToGroundContext editDropToGroundContext{
                .canDrop = !selection_.selection().empty(),
                .drop = [this] { dropSelectedToGroundPlane(); },
            };
            UI::MenuBar::drawEditDropToGround(editDropToGroundContext);
            const UI::EditSnapSelectionToGridContext editSnapSelectionToGridContext{
                .canSnap = !selection_.selection().empty(),
                .snap = [this] {
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
                },
            };
            UI::MenuBar::drawEditSnapSelectionToGrid(editSnapSelectionToGridContext);
            const UI::EditMirrorSelectionContext editMirrorSelectionContext{
                .canMirror = !selection_.selection().empty(),
                .mirror = [this](int axis) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (objectLockState_.isLocked(s->id)) continue;
                        s->transform.scale[axis] = -s->transform.scale[axis];
                    }
                    modified_ = true; updateWindowTitle();
                    char mbuf[48];
                    std::snprintf(mbuf, sizeof(mbuf), "Mirrored %d object(s) on %c",
                                  static_cast<int>(selection_.selection().size()),
                                  "XYZ"[axis]);
                    setStatusMsg(mbuf);
                },
            };
            UI::MenuBar::drawEditMirrorSelection(editMirrorSelectionContext);
            const UI::EditGroupScaleContext editGroupScaleContext{
                .canScale = selection_.selection().size() >= 2,
                .openDialog = [this] { groupScaleOpen_ = true; },
            };
            UI::MenuBar::drawEditGroupScale(editGroupScaleContext);
            const UI::EditLinearArrayContext editLinearArrayContext{
                .canOpen = !selection_.selection().empty(),
                .openDialog = [this] { arrayDupOpen_ = true; },
            };
            UI::MenuBar::drawEditLinearArray(editLinearArrayContext);
            const UI::EditScatterAlongCurveContext editScatterAlongCurveContext{
                .canOpen = !selection_.selection().empty(),
                .openDialog = [this] { scatterCurveOpen_ = true; },
            };
            UI::MenuBar::drawEditScatterAlongCurve(editScatterAlongCurveContext);
            const UI::EditBatchRenameContext editBatchRenameContext{
                .canOpen = !selection_.selection().empty(),
                .openDialog = [this] { batchRenameOpen_ = true; },
            };
            UI::MenuBar::drawEditBatchRename(editBatchRenameContext);
            const UI::EditFindReplaceNamesContext editFindReplaceNamesContext{
                .openDialog = [this] { findReplaceOpen_ = true; },
            };
            UI::MenuBar::drawEditFindReplaceNames(editFindReplaceNamesContext);
            const UI::EditRandomizeTransformContext editRandomizeTransformContext{
                .canOpen = !selection_.selection().empty(),
                .openDialog = [this] { randomizeOpen_ = true; },
            };
            UI::MenuBar::drawEditRandomizeTransform(editRandomizeTransformContext);
            ImGui::Separator();
            const UI::EditMacroRecordingContext editMacroRecordingContext{
                .isRecording = macroRecorder_.isRecording(),
                .startRecording = [this] {
                    macroRecorder_.startRecording();
                    setStatusMsg("Recording started — edit the scene, then Stop", false, 3.f);
                },
                .stopRecording = [this] {
                    macroRecorder_.stopRecording();
                    setStatusMsg("Recording stopped", false, 2.f);
                },
            };
            UI::MenuBar::drawEditMacroRecording(editMacroRecordingContext);
            const UI::EditPlayMacroContext editPlayMacroContext{
                .canPlay = macroRecorder_.stepCount() > 0 && !macroRecorder_.isRecording(),
                .play = [this] { macroRecorder_.play(macroContext()); },
            };
            UI::MenuBar::drawEditPlayMacro(editPlayMacroContext);
            const UI::EditMacroEditorContext editMacroEditorContext{
                .openDialog = [this] { macroOpen_ = true; },
            };
            UI::MenuBar::drawEditMacroEditor(editMacroEditorContext);
            ImGui::Separator();
            const UI::EditLockSelectionContext editLockSelectionContext{
                .canToggle = !selection_.selection().empty(),
                .toggle = [this] {
                    for (const auto& s : selection_.selection()) {
                        objectLockState_.toggle(s->id);
                    }
                },
            };
            UI::MenuBar::drawEditLockSelection(editLockSelectionContext);
            ImGui::Separator();
            bool hasSel = !selection_.selection().empty();
            const UI::EditResetTransformContext editResetTransformContext{
                .canReset = hasSel,
                .reset = [this](UI::EditResetTransformTarget target) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (objectLockState_.isLocked(s->id)) continue;
                        switch (target) {
                            case UI::EditResetTransformTarget::Position:
                                s->transform.position = {0.0f, 0.0f, 0.0f};
                                break;
                            case UI::EditResetTransformTarget::Rotation:
                                s->transform.rotation = {0.0f, 0.0f, 0.0f};
                                break;
                            case UI::EditResetTransformTarget::Scale:
                                s->transform.scale = {1.0f, 1.0f, 1.0f};
                                break;
                            case UI::EditResetTransformTarget::All:
                                s->transform.position = {0.0f, 0.0f, 0.0f};
                                s->transform.rotation = {0.0f, 0.0f, 0.0f};
                                s->transform.scale    = {1.0f, 1.0f, 1.0f};
                                break;
                        }
                    }
                    modified_ = true; updateWindowTitle();
                },
            };
            UI::MenuBar::drawEditResetTransform(editResetTransformContext);
            ImGui::Separator();
            const UI::EditTransformClipboardContext editTransformClipboardContext{
                .canCopy = hasSel,
                .canPaste = hasSel && transformClipboard_.hasValue(),
                .copy = [this] {
                    const auto& src = selection_.selection().front();
                    transformClipboard_.copyFrom(*src);
                    setStatusMsg("Transform copied from \"" + src->name + "\"");
                },
                .paste = [this] {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (objectLockState_.isLocked(s->id)) continue;
                        transformClipboard_.pasteTo(*s);
                    }
                    modified_ = true; updateWindowTitle();
                    setStatusMsg("Transform pasted to " + std::to_string(selection_.selection().size()) + " object(s)");
                },
            };
            UI::MenuBar::drawEditTransformClipboard(editTransformClipboardContext);
            ImGui::Separator();
            const UI::EditIsolateSelectionContext editIsolateSelectionContext{
                .isActive = isolateActive_,
                .canToggle = isolateActive_ || !selection_.selection().empty(),
                .toggle = [this] { toggleIsolate(); },
            };
            UI::MenuBar::drawEditIsolateSelection(editIsolateSelectionContext);
            ImGui::Separator();
            const UI::EditHideSelectionContext editHideSelectionContext{
                .canHide = !selection_.selection().empty(),
                .hide = [this] {
                    auto sel = selection_.selection();
                    pushUndo();
                    for (auto& s : sel) s->visible = false;
                    selection_.clear();
                    modified_ = true; updateWindowTitle();
                },
            };
            UI::MenuBar::drawEditHideSelection(editHideSelectionContext);
            const UI::EditShowAllHiddenContext editShowAllHiddenContext{
                .showAll = [this, &walkAll] {
                    pushUndo();
                    walkAll(document_.objects, [&](const auto& o) { o->visible = true; });
                    modified_ = true; updateWindowTitle();
                },
            };
            UI::MenuBar::drawEditShowAllHidden(editShowAllHiddenContext);
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

void MenuBar::drawFileMergeScene(const FileMergeSceneContext& context) {
    if (ImGui::MenuItem("Merge Scene...")) context.openDialog();
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

void MenuBar::drawEditGrouping(const EditGroupingContext& context) {
    if (ImGui::MenuItem("Group", "Ctrl+G")) context.group();
    if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G")) context.ungroup();
}

void MenuBar::drawEditConvertToDefinition(const EditConvertToDefinitionContext& context) {
    if (ImGui::MenuItem("Convert to Definition", nullptr, false, context.canConvert)) {
        context.convert();
    }
}

void MenuBar::drawEditExportSubtree(const EditExportSubtreeContext& context) {
    if (ImGui::MenuItem("Export Subtree as Template...", nullptr, false, context.canExport)) {
        context.openDialog();
    }
}

void MenuBar::drawEditBreakInstance(const EditBreakInstanceContext& context) {
    if (ImGui::MenuItem("Break Instance", nullptr, false, context.canBreak)) {
        context.breakInstance();
    }
}

void MenuBar::drawEditAlignSelection(const EditAlignSelectionContext& context) {
    if (!ImGui::BeginMenu("Align Selection", context.canAlign)) return;

    static const char* minimumLabels[3] = {
        "Min X (left)", "Min Y (bottom)", "Min Z (front)"
    };
    static const char* centerLabels[3] = {"Center X", "Center Y", "Center Z"};
    static const char* maximumLabels[3] = {
        "Max X (right)", "Max Y (top)", "Max Z (back)"
    };

    for (int axis = 0; axis < 3; ++axis) {
        if (axis > 0) ImGui::Separator();
        if (ImGui::MenuItem(minimumLabels[axis])) {
            context.align(axis, EditAlignmentTarget::Minimum);
        }
        if (ImGui::MenuItem(centerLabels[axis])) {
            context.align(axis, EditAlignmentTarget::Center);
        }
        if (ImGui::MenuItem(maximumLabels[axis])) {
            context.align(axis, EditAlignmentTarget::Maximum);
        }
    }

    ImGui::Separator();
    if (ImGui::MenuItem("To First Selected (XYZ)", nullptr, false,
                        context.canAlignToFirst)) {
        context.alignToFirst();
    }
    ImGui::EndMenu();
}

void MenuBar::drawEditDistributeSelection(const EditDistributeSelectionContext& context) {
    if (!ImGui::BeginMenu("Distribute Selection", context.canDistribute)) return;

    static const char* labels[3] = {
        "Distribute X (spacing)",
        "Distribute Y (spacing)",
        "Distribute Z (spacing)",
    };
    for (int axis = 0; axis < 3; ++axis) {
        if (ImGui::MenuItem(labels[axis])) context.distribute(axis);
    }
    ImGui::EndMenu();
}

void MenuBar::drawEditDropToGround(const EditDropToGroundContext& context) {
    if (ImGui::MenuItem("Drop to Ground Plane", nullptr, false, context.canDrop)) {
        context.drop();
    }
}

void MenuBar::drawEditSnapSelectionToGrid(const EditSnapSelectionToGridContext& context) {
    if (ImGui::MenuItem("Snap Selection to Grid", nullptr, false, context.canSnap)) {
        context.snap();
    }
}

void MenuBar::drawEditMirrorSelection(const EditMirrorSelectionContext& context) {
    if (!ImGui::BeginMenu("Mirror Selection", context.canMirror)) return;

    static const char* labels[3] = { "Flip X", "Flip Y", "Flip Z" };
    for (int axis = 0; axis < 3; ++axis) {
        if (ImGui::MenuItem(labels[axis])) context.mirror(axis);
    }
    ImGui::EndMenu();
}

void MenuBar::drawEditGroupScale(const EditGroupScaleContext& context) {
    if (ImGui::MenuItem("Group Scale…", nullptr, false, context.canScale)) {
        context.openDialog();
    }
}

void MenuBar::drawEditLinearArray(const EditLinearArrayContext& context) {
    if (ImGui::MenuItem("Linear Array...", nullptr, false, context.canOpen)) {
        context.openDialog();
    }
}

void MenuBar::drawEditScatterAlongCurve(const EditScatterAlongCurveContext& context) {
    if (ImGui::MenuItem("Scatter Along Curve...", nullptr, false, context.canOpen)) {
        context.openDialog();
    }
}

void MenuBar::drawEditBatchRename(const EditBatchRenameContext& context) {
    if (ImGui::MenuItem("Batch Rename...", "Ctrl+Shift+R", false, context.canOpen)) {
        context.openDialog();
    }
}

void MenuBar::drawEditFindReplaceNames(const EditFindReplaceNamesContext& context) {
    if (ImGui::MenuItem("Find & Replace Names...", "Ctrl+H")) {
        context.openDialog();
    }
}

void MenuBar::drawEditRandomizeTransform(const EditRandomizeTransformContext& context) {
    if (ImGui::MenuItem("Randomize Transform...", nullptr, false, context.canOpen)) {
        context.openDialog();
    }
}

void MenuBar::drawEditMacroRecording(const EditMacroRecordingContext& context) {
    if (context.isRecording) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.f));
        if (ImGui::MenuItem("Stop Recording")) context.stopRecording();
        ImGui::PopStyleColor();
    } else if (ImGui::MenuItem("Record Macro")) {
        context.startRecording();
    }
}

void MenuBar::drawEditPlayMacro(const EditPlayMacroContext& context) {
    if (ImGui::MenuItem("Play Macro", nullptr, false, context.canPlay)) context.play();
}

void MenuBar::drawEditMacroEditor(const EditMacroEditorContext& context) {
    if (ImGui::MenuItem("Macro Editor…")) context.openDialog();
}

void MenuBar::drawEditLockSelection(const EditLockSelectionContext& context) {
    if (ImGui::MenuItem("Lock/Unlock Selected", "Ctrl+L", false, context.canToggle)) {
        context.toggle();
    }
}

void MenuBar::drawEditResetTransform(const EditResetTransformContext& context) {
    if (!ImGui::BeginMenu("Reset Transform", context.canReset)) return;

    if (ImGui::MenuItem("Position", "Alt+G")) {
        context.reset(EditResetTransformTarget::Position);
    }
    if (ImGui::MenuItem("Rotation", "Alt+R")) {
        context.reset(EditResetTransformTarget::Rotation);
    }
    if (ImGui::MenuItem("Scale", "Alt+S")) {
        context.reset(EditResetTransformTarget::Scale);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("All")) context.reset(EditResetTransformTarget::All);
    ImGui::EndMenu();
}

void MenuBar::drawEditTransformClipboard(const EditTransformClipboardContext& context) {
    if (ImGui::MenuItem("Copy Transform", "Ctrl+Shift+C", false, context.canCopy)) {
        context.copy();
    }
    if (ImGui::MenuItem("Paste Transform", "Ctrl+Shift+V", false, context.canPaste)) {
        context.paste();
    }
}

void MenuBar::drawEditIsolateSelection(const EditIsolateSelectionContext& context) {
    if (ImGui::MenuItem(context.isActive ? "Exit Isolation" : "Isolate Selection",
                        "Alt+I", false, context.canToggle)) {
        context.toggle();
    }
}

void MenuBar::drawEditHideSelection(const EditHideSelectionContext& context) {
    if (ImGui::MenuItem("Hide Selected", "H", false, context.canHide)) {
        context.hide();
    }
}

void MenuBar::drawEditShowAllHidden(const EditShowAllHiddenContext& context) {
    if (ImGui::MenuItem("Show All Hidden", "Alt+H")) context.showAll();
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
