#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include "MeshCraft/Application/UI/CameraPresetOverlay.hpp"
#include "MeshCraft/MeshCraftPrivate.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"
#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"

#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"

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

namespace MeshCraft::Application {

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Graphics;


void MeshCraftApplication::drawStatsOverlay(int screenW, [[maybe_unused]] int screenH)
{
    if (showStatsOverlay_) {
        int tlH2 = showTimeline_ ? kTimelineH : 0;
        float vpRight = static_cast<float>(screenW - kRightPanelW);
        float vpTop   = static_cast<float>(imguiTopH_);

        // Count objects (total / visible / locked)
        int totalObjs = 0, visibleObjs = 0, lockedObjs = 0;
        std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> countStats;
        countStats = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
            for (const auto& o : list) {
                ++totalObjs;
                if (o->visible)                ++visibleObjs;
                if (objectLockState_.isLocked(o->id)) ++lockedObjs;
                countStats(o->children);
            }
        };
        countStats(document_.objects);
        int selCount = static_cast<int>(selection_.selection().size());

        ImGui::SetNextWindowPos(ImVec2(vpRight - 8.0f, vpTop + 8.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.50f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
        ImGui::Begin("##statsoverlay", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav        | ImGuiWindowFlags_NoMove   |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%.0f FPS", displayFps_);
        if (isolateActive_)
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.1f, 1.0f), "ISOLATED");
        if (lookThroughCamera_ && selectedCameraIdx_ >= 0 &&
            selectedCameraIdx_ < static_cast<int>(document_.cameras.size()))
            ImGui::TextColored(ImVec4(0.75f, 0.45f, 1.0f, 1.0f), "CAM: %s",
                               document_.cameras[selectedCameraIdx_].name.c_str());
        ImGui::Separator();
        ImGui::Text("Objects: %d", totalObjs);
        ImGui::Text("Visible: %d", visibleObjs);
        if (lockedObjs > 0) ImGui::Text("Locked:  %d", lockedObjs);
        ImGui::Separator();
        if (selCount > 0)
            ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f), "Selected: %d", selCount);
        else
            ImGui::TextDisabled("Selected: 0");
        ImGui::Separator();
        ImGui::TextDisabled("Cam dist: %.2f", camera_.distance);
        ImGui::TextDisabled("Target: %.1f, %.1f, %.1f",
            camera_.target.X, camera_.target.Y, camera_.target.Z);
        ImGui::Separator();
        {
            int sv = 0, st = 0;
            sceneRenderer_->scenePolyStats(document_, sv, st);
            ImGui::TextDisabled("Verts: %d", sv);
            ImGui::TextDisabled("Tris:  %d", st);
        }
        ImGui::End();
        ImGui::PopStyleVar();
        (void)tlH2;
    }

    // Gizmo drag delta overlay — shown near the mouse cursor while dragging
    if (gizmo_.isDragging() && selection_.hasSelection()) {
        const auto& sel0 = *selection_.selection().front();
        int axIdx = gizmoDragAxisIdx_;
        float curVal = 0.0f;
        const char* unit = "";
        const char* axName = "XYZ"[axIdx] == 'X' ? "X" : ("XYZ"[axIdx] == 'Y' ? "Y" : "Z");
        static const char* kAxis[3] = {"X","Y","Z"};

        switch (activeTool_) {
        case ActiveTool::Move:
            curVal = sel0.transform.position[axIdx];
            unit   = " u";
            break;
        case ActiveTool::Rotate:
            curVal = sel0.transform.rotation[axIdx];
            unit   = "°";
            break;
        case ActiveTool::Scale:
            curVal = sel0.transform.scale[axIdx];
            unit   = "";
            break;
        default: break;
        }
        float delta = curVal - gizmoDragStartVal_;

        ImVec2 mp = ImGui::GetIO().MousePos;
        ImGui::SetNextWindowPos(ImVec2(mp.x + 18.0f, mp.y - 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
        ImGui::Begin("##gizmoDelta", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav        | ImGuiWindowFlags_NoMove   |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        // Axis label colored
        static const ImVec4 kAxisCol[3] = {
            {1.0f, 0.25f, 0.25f, 1.0f},
            {0.25f, 1.0f, 0.25f, 1.0f},
            {0.25f, 0.55f, 1.0f, 1.0f}
        };
        ImGui::TextColored(kAxisCol[axIdx], "%s", kAxis[axIdx]);
        ImGui::SameLine(0, 4);
        if (delta >= 0.0f)
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "+%.4g%s", delta, unit);
        else
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.8f, 1.0f), "%.4g%s", delta, unit);
        ImGui::SameLine(0, 6);
        ImGui::TextDisabled("(%.4g)", curVal);
        // Show snap indicator for rotate when Ctrl or snap grid is active
        if (activeTool_ == ActiveTool::Rotate) {
            bool ctrlDown = ImGui::GetIO().KeyCtrl;
            if (ctrlDown || snapEnabled_) {
                ImGui::SameLine(0, 6);
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "[snap %.4g\xc2\xb0]", snapRotate_);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
        (void)axName;
    }

    // Measurement tool overlay
    if (activeTool_ == ActiveTool::Measure && mPt1Set_) {
        // Project 3D world point to screen using cached VP matrix
        auto w2s = [&](float wx, float wy, float wz) -> ImVec2 {
            float cX = wx*cachedVP_.M11 + wy*cachedVP_.M21 + wz*cachedVP_.M31 + cachedVP_.M41;
            float cY = wx*cachedVP_.M12 + wy*cachedVP_.M22 + wz*cachedVP_.M32 + cachedVP_.M42;
            float cW = wx*cachedVP_.M14 + wy*cachedVP_.M24 + wz*cachedVP_.M34 + cachedVP_.M44;
            if (std::abs(cW) < 1e-6f) return {-9999, -9999};
            float sx = (cX/cW * 0.5f + 0.5f) * cachedVW_ + cachedVX_;
            float sy = (1.0f - (cY/cW * 0.5f + 0.5f)) * cachedVH_ + cachedVY_;
            return {sx, sy};
        };

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        ImVec2 s1 = w2s(mPt1_[0], mPt1_[1], mPt1_[2]);
        // Draw point 1 marker
        dl->AddCircleFilled(s1, 5.0f, IM_COL32(80, 220, 220, 220));

        if (mPt2Set_) {
            ImVec2 s2 = w2s(mPt2_[0], mPt2_[1], mPt2_[2]);
            // Draw line and point 2
            dl->AddLine(s1, s2, IM_COL32(80, 220, 220, 200), 2.0f);
            dl->AddCircleFilled(s2, 5.0f, IM_COL32(80, 220, 220, 220));

            // Distance label near midpoint
            float mx2 = (s1.x + s2.x) * 0.5f, my2 = (s1.y + s2.y) * 0.5f;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.4g u", mDist_);
            dl->AddText(ImVec2(mx2 + 8, my2 - 8), IM_COL32(200, 255, 200, 255), buf);
        } else {
            // Show hint
            ImGui::SetNextWindowPos(ImVec2(s1.x + 12, s1.y - 24), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.60f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 3));
            ImGui::Begin("##mhint", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::TextDisabled("Click second point");
            ImGui::End();
            ImGui::PopStyleVar();
        }

        // Info panel bottom-left of viewport
        if (mPt2Set_) {
            ImGui::SetNextWindowPos(ImVec2(static_cast<float>(cachedVX_) + 8,
                                          static_cast<float>(cachedVY_ + cachedVH_) - 8),
                                    ImGuiCond_Always, ImVec2(0.0f, 1.0f));
            ImGui::SetNextWindowBgAlpha(0.65f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            ImGui::Begin("##minfo", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::TextColored(ImVec4(0.4f,1.0f,1.0f,1.0f), "Distance: %.4g u", mDist_);
            ImGui::TextDisabled("A: %.2f, %.2f, %.2f", mPt1_[0], mPt1_[1], mPt1_[2]);
            ImGui::TextDisabled("B: %.2f, %.2f, %.2f", mPt2_[0], mPt2_[1], mPt2_[2]);
            ImGui::TextDisabled("Right-click to reset");
            ImGui::End();
            ImGui::PopStyleVar();
        }
    }

    // Camera preset buttons — top-left corner of the 3D viewport.
    const bool hasSelectedCamera = selectedCameraIdx_ >= 0 &&
        selectedCameraIdx_ < static_cast<int>(document_.cameras.size());
    const UI::CameraPresetOverlayContext cameraPresetContext{
        .orthographic = camera_.orthographic,
        .hasSelectedCamera = hasSelectedCamera,
        .lookingThroughCamera = lookThroughCamera_,
        .selectedCameraName = hasSelectedCamera
            ? std::string_view(document_.cameras[selectedCameraIdx_].name)
            : std::string_view{},
        .resetCamera = [this] { camera_.reset(); camera_.orthographic = false; },
        .setOrbitDirection = [this](float yaw, float pitch) {
            camera_.yaw = yaw;
            camera_.pitch = pitch;
        },
        .setOrthographic = [this](bool enabled) { camera_.orthographic = enabled; },
        .toggleLookThroughCamera = [this] { lookThroughCamera_ = !lookThroughCamera_; },
    };
    UI::CameraPresetOverlay::draw(static_cast<float>(kLeftPanelW) + 8.0f,
                                  static_cast<float>(imguiTopH_) + 8.0f,
                                  cameraPresetContext);
}

void MeshCraftApplication::drawStatusBar(int screenW, int screenH)
{
    (void)screenW;
    // Status bar (bottom)
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(0, static_cast<float>(screenH - kStatusH)));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(screenW), static_cast<float>(kStatusH)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 3));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f, 0.094f, 0.176f, 1.0f));
    ImGui::Begin("##statusbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);
    {
        // Timed notification takes priority; falls back to scene info
        if (statusNotification_.active()) {
            ImVec4 col = statusNotification_.isError() ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f)
                                                        : ImVec4(0.55f, 1.0f, 0.55f, 1.0f);
            ImGui::TextColored(col, "%s", statusNotification_.message().c_str());
        } else {
            int totalObjs = 0;
            std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> countAll =
                [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
                    totalObjs += static_cast<int>(list.size());
                    for (const auto& o : list) countAll(o->children);
                };
            countAll(document_.objects);
            int selCount = static_cast<int>(selection_.selection().size());
            if (selCount > 0) {
                const std::string& selName = selection_.selection().front()->name;
                ImGui::Text("%d objects | %d selected | %s", totalObjs, selCount, selName.c_str());
            } else {
                ImGui::Text("%d objects", totalObjs);
            }
        }

        // SYS-W14-02: small clickable indicator for the last load/save/export's
        // validation findings -- otherwise SYS-W1-01's diagnostics are console-only.
        if (!lastValidation_.empty()) {
            const std::string label =
                "[!] " + std::to_string(lastValidation_.entries.size()) + " validation note(s)";
            ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - 12.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "%s", label.c_str());
            if (ImGui::IsItemClicked()) showValidationPanel_ = true;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("From: %s\nClick to open the Validation panel",
                                   lastValidationSource_.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();


}

void MeshCraftApplication::drawDialogs()
{
    // -----------------------------------------------------------------------
    // Command Palette (Ctrl+P)
    // -----------------------------------------------------------------------
    if (cmdPaletteOpen_) {
        ImGui::OpenPopup("##cmdpalette");
        cmdPaletteOpen_ = false;
        cmdPaletteBuf_[0] = '\0';
    }
    {
        ImVec2 dsp = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(
            ImVec2(dsp.x * 0.5f, static_cast<float>(imguiTopH_) + 10.0f),
            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.97f);
    }
    if (ImGui::BeginPopup("##cmdpalette",
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoSavedSettings)) {

        // Search input — auto-focus when the popup first appears
        ImGui::SetNextItemWidth(-1);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##cpsearch", "Search commands or objects…",
                                 cmdPaletteBuf_, sizeof(cmdPaletteBuf_));

        std::string filt(cmdPaletteBuf_);
        for (auto& c : filt) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        bool hasFilter = !filt.empty();

        ImGui::Separator();

        bool closePalette = false;
        int  shown = 0;
        constexpr int kMaxShown = 20;

        // Helper: show one entry; returns true if clicked
        auto entry = [&](const char* cat, ImVec4 catCol,
                         const char* label, const char* hint = nullptr) -> bool {
            if (closePalette || shown >= kMaxShown) return false;
            // Filter: skip if doesn't match (cat + label checked)
            if (hasFilter) {
                std::string lblLow(label);
                for (auto& c : lblLow) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (lblLow.find(filt) == std::string::npos) return false;
            }
            ++shown;
            ImGui::PushID(shown);
            ImGui::TextColored(catCol, "%-6s", cat);
            ImGui::SameLine(0, 6);
            bool clicked = ImGui::Selectable(label, false,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
            if (hint) {
                ImGui::SameLine();
                float rightEdge = ImGui::GetWindowWidth() - ImGui::CalcTextSize(hint).x - 12.0f;
                ImGui::SetCursorPosX(rightEdge);
                ImGui::TextDisabled("%s", hint);
            }
            ImGui::PopID();
            if (clicked) closePalette = true;
            return clicked;
        };

        ImVec4 cFile(0.55f, 0.85f, 0.55f, 1.0f);
        ImVec4 cEdit(1.0f,  0.80f, 0.40f, 1.0f);
        ImVec4 cAdd (0.55f, 0.75f, 1.0f,  1.0f);
        ImVec4 cObj (0.80f, 0.80f, 0.80f, 1.0f);
        ImVec4 cSel (1.0f,  0.60f, 0.60f, 1.0f);

        bool hasSel = selection_.hasSelection();
        bool hasUndo = undoManager_.canUndo();
        bool hasRedo = undoManager_.canRedo();

        // ---- File commands ----
        if (entry(  "File", cFile, "New Scene",        "Ctrl+N"))   confirmIfModified(PendingAction::NewScene);
        if (entry(  "File", cFile, "Open...",          "Ctrl+O"))   confirmIfModified(PendingAction::OpenFile);
        if (entry(  "File", cFile, "Save",             "Ctrl+S"))   saveFile();
        if (entry(  "File", cFile, "Save As...",       "Ctrl+⇧S"))  saveFileAs();
        if (entry(  "File", cFile, "Export GLB",       "Ctrl+E"))   exportGltf();
        if (entry(  "File", cFile, "Screenshot",       "F11"))      saveScreenshot("screenshot.ppm");

        // ---- Edit commands ----
        if (hasUndo && entry("Edit", cEdit, "Undo",   "Ctrl+Z")) performUndo();
        if (hasRedo && entry("Edit", cEdit, "Redo",   "Ctrl+Y")) performRedo();
        if (entry("Edit", cEdit, "Select All",         "Ctrl+A")) {
            selection_.clear();
            for (auto& o : document_.objects) selection_.select(o);
            updateWindowTitle();
        }
        if (entry("Edit", cEdit, "Deselect All",       "Esc"))    { selection_.clear(); updateWindowTitle(); }
        if (hasSel) {
            if (entry("Edit", cEdit, "Delete Selected",    "Del"))  deleteSelected();
            if (entry("Edit", cEdit, "Duplicate",      "Ctrl+D"))   duplicateSelected();
            if (entry("Edit", cEdit, "Linear Array…",      ""))     arrayDupOpen_ = true;
            if (entry("Edit", cEdit, "Group",          "Ctrl+G"))   groupSelected();
            if (entry("Edit", cEdit, "Ungroup",     "Ctrl+⇧G"))     ungroupSelected();
            if (entry("Edit", cEdit, "Batch Rename…", "Ctrl+⇧R"))   batchRenameOpen_ = true;
            if (entry("Edit", cEdit, "Find & Replace Names…", "Ctrl+H")) findReplaceOpen_ = true;
            if (entry("Edit", cEdit, "Randomize Transform…", ""))   randomizeOpen_ = true;
            if (selection_.selection().size() >= 2)
                if (entry("Edit", cEdit, "Copy Properties to Selected…", "Ctrl+⇧P")) copyPropsOpen_ = true;
            if (hasSel) {
                if (entry("Edit", cEdit, "Drop to Ground Plane", "")) dropSelectedToGroundPlane();
            }
            if (entry("Edit", cEdit, "Select Parent",  "P"))        selectParent();
            if (hasSel && entry("Edit", cEdit, "Convert to Definition", "")) convertToDefinition();
            if (hasSel && !selection_.selection().empty() &&
                selection_.selection().front()->type == Mc3::ObjectType::Instance)
                if (entry("Edit", cEdit, "Break Instance", "")) breakInstance();
            if (entry("Edit", cEdit, "Select Children", ""))        selectChildren();
            if (selection_.selection().size() >= 2)
                if (entry("Edit", cEdit, "Align to First Selected", "")) alignToObject();
        }

        // ---- Add commands ----
        if (entry("Add", cAdd, "Add Box",       "F1")) addPrimitive(Mc3::ObjectType::Box);
        if (entry("Add", cAdd, "Add Sphere",    "F2")) addPrimitive(Mc3::ObjectType::Sphere);
        if (entry("Add", cAdd, "Add Cylinder",  "F3")) addPrimitive(Mc3::ObjectType::Cylinder);
        if (entry("Add", cAdd, "Add Cone",      "F4")) addPrimitive(Mc3::ObjectType::Cone);
        if (entry("Add", cAdd, "Add Plane",     "F5")) addPrimitive(Mc3::ObjectType::Plane);
        if (entry("Add", cAdd, "Add Group",     ""))   addPrimitive(Mc3::ObjectType::Group);
        if (entry("Add", cAdd, "Add Extrude",   ""))   addPrimitive(Mc3::ObjectType::Extrude);

        // ---- View commands ----
        if (entry("View", cFile, "Toggle Stats Overlay",   "")) showStatsOverlay_ = !showStatsOverlay_;
        if (entry("View", cFile, "Toggle Edge Overlay", "Alt+W")) showEdgeOverlay_ = !showEdgeOverlay_;
        if (entry("View", cFile, "Toggle Wireframe Mode", ""))    showWireframeMode_ = !showWireframeMode_;
        if (entry("View", cFile, "Toggle Timeline",  "Ctrl+T"))  showTimeline_ = !showTimeline_;
        if (entry("View", cFile, "Toggle Snap to Grid",  ""))    snapEnabled_  = !snapEnabled_;

        // ---- Scene objects (only shown when filtering) ----
        if (hasFilter) {
            if (shown > 0 && shown < kMaxShown) ImGui::Separator();

            // Recursively walk objects; build breadcrumb path and match name/id/tags
            std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&, const std::string&)> addObjs;
            addObjs = [&](const auto& list, const std::string& parentPath) {
                for (const auto& obj : list) {
                    const std::string& displayName = obj->name.empty() ? obj->id : obj->name;

                    // Determine type prefix (same as hierarchy panel)
                    const char* tp = "";
                    if      (obj->type == Mc3::ObjectType::Box || obj->type == Mc3::ObjectType::Cube) tp = "[B] ";
                    else if (obj->type == Mc3::ObjectType::Sphere)       tp = "[S] ";
                    else if (obj->type == Mc3::ObjectType::Cylinder)     tp = "[C] ";
                    else if (obj->type == Mc3::ObjectType::Cone)         tp = "[K] ";
                    else if (obj->type == Mc3::ObjectType::Plane)        tp = "[P] ";
                    else if (obj->type == Mc3::ObjectType::Torus)        tp = "[T] ";
                    else if (obj->type == Mc3::ObjectType::Mesh)         tp = "[M] ";
                    else if (obj->type == Mc3::ObjectType::Extrude)      tp = "[E] ";
                    else if (obj->type == Mc3::ObjectType::Instance)     tp = "[i] ";
                    else if (obj->type == Mc3::ObjectType::Group)        tp = "[G] ";
                    else if (obj->type == Mc3::ObjectType::Union)        tp = "[U] ";
                    else if (obj->type == Mc3::ObjectType::Difference)   tp = "[D] ";
                    else if (obj->type == Mc3::ObjectType::Intersection) tp = "[X] ";

                    // Build label: "prefix + name" and hint: breadcrumb path
                    std::string label = std::string(tp) + displayName;
                    std::string path = parentPath.empty() ? displayName : parentPath + " / " + displayName;

                    // Match: check label AND tags
                    std::string labelLow = label;
                    for (auto& c : labelLow) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    bool tagMatch = false;
                    for (const auto& tag : obj->tags) {
                        std::string tl = tag;
                        for (auto& c : tl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (tl.find(filt) != std::string::npos) { tagMatch = true; break; }
                    }

                    if (labelLow.find(filt) != std::string::npos || tagMatch) {
                        // Show hint as parent path (right-aligned)
                        if (shown < kMaxShown) {
                            ++shown;
                            ImGui::PushID(obj->id.c_str());
                            ImGui::TextColored(cObj, "%-6s", "Object");
                            ImGui::SameLine(0, 6);
                            bool clicked = ImGui::Selectable(label.c_str(), false,
                                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                            if (!parentPath.empty()) {
                                ImGui::SameLine();
                                float rightEdge = ImGui::GetWindowWidth() - ImGui::CalcTextSize(parentPath.c_str()).x - 12.0f;
                                if (rightEdge > ImGui::GetCursorPosX() + 6.0f) {
                                    ImGui::SetCursorPosX(rightEdge);
                                    ImGui::TextDisabled("%s", parentPath.c_str());
                                }
                            }
                            ImGui::PopID();
                            if (clicked) {
                                selection_.clear();
                                selection_.select(obj);
                                hierarchyPanel_->scrollToObject(obj->id);
                                updateWindowTitle();
                                closePalette = true;
                            }
                        }
                    }

                    addObjs(obj->children, path);
                }
            };
            addObjs(document_.objects, "");
        }

        if (shown == 0) ImGui::TextDisabled("No results for \"%s\"", cmdPaletteBuf_);

        if (closePalette || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Batch rename dialog
    // -----------------------------------------------------------------------
    if (batchRenameOpen_) {
        ImGui::OpenPopup("Batch Rename##brdlg");
        batchRenameOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Batch Rename##brdlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        int selCount = static_cast<int>(selection_.selection().size());
        ImGui::Text("%d object(s) selected", selCount);
        ImGui::Separator();
        ImGui::TextDisabled("Variables: {name}  {index}  {index:02d}  {index0}  {type}");
        ImGui::Spacing();
        ImGui::Text("Pattern:");
        ImGui::SetNextItemWidth(380);
        ImGui::InputText("##brpat", batchRenameBuf_, sizeof(batchRenameBuf_));

        // Live preview (first 3 objects)
        if (batchRenameBuf_[0] != '\0' && !selection_.selection().empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Preview:");
            int previewN = std::min(selCount, 3);
            for (int i = 0; i < previewN; ++i) {
                const auto& obj = selection_.selection()[static_cast<size_t>(i)];
                // AUD-035: batchRenameObjects (the real Apply path) skips
                // locked objects entirely -- the preview must match, or a
                // locked object among the first 3 selected shows a rename
                // here that will not actually happen on Apply. Locked-
                // object index numbering still advances in Apply (see
                // batchRenameObjects's unconditional `++idx`), so `i + 1`
                // here already lines up positionally; only the locked
                // object's own row needs to stop rendering a rename.
                if (objectLockState_.isLocked(obj->id)) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                        "  %s  (locked, skipped)", obj->name.c_str());
                    continue;
                }
                std::string preview = applyRenamePatternAlg(
                    batchRenameBuf_, obj->name, i + 1, objectTypeName(obj->type));
                ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f),
                    "  %s  →  %s", obj->name.c_str(), preview.c_str());
            }
            if (selCount > 3)
                ImGui::TextDisabled("  … and %d more", selCount - 3);
        }
        ImGui::Spacing();
        ImGui::Separator();
        bool canApply = batchRenameBuf_[0] != '\0' && !selection_.selection().empty();
        if (!canApply) ImGui::BeginDisabled();
        if (ImGui::Button("Rename", ImVec2(100, 0)) || (canApply && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            batchRenameSelected();
            ImGui::CloseCurrentPopup();
        }
        if (!canApply) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Randomize Transform dialog
    // -----------------------------------------------------------------------
    if (randomizeOpen_) {
        ImGui::OpenPopup("Randomize Transform##rtdlg");
        randomizeOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Randomize Transform##rtdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        int selCount = static_cast<int>(selection_.selection().size());
        ImGui::Text("%d object(s) selected", selCount);
        ImGui::TextDisabled("Offsets are ADDED to existing transforms. Locked objects skipped.");
        ImGui::Separator();

        // AlwaysClamp (AUDIT-0044): these are "±range" values -- a
        // Ctrl+Click-typed negative range is semantically nonsensical and
        // could break the underlying random-distribution math downstream.
        ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f), "Position offset (±units)");
        ImGui::SetNextItemWidth(260);
        ImGui::SliderFloat("±X pos##rx", &scatterPosRange_[0], 0.0f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(260);
        ImGui::SliderFloat("±Y pos##ry", &scatterPosRange_[1], 0.0f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(260);
        ImGui::SliderFloat("±Z pos##rz", &scatterPosRange_[2], 0.0f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Rotation offset (±degrees)");
        ImGui::SetNextItemWidth(260);
        ImGui::SliderFloat("±X rot##rrx", &scatterRotRange_[0], 0.0f, 180.0f, "%.1f°", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(260);
        ImGui::SliderFloat("±Y rot##rry", &scatterRotRange_[1], 0.0f, 180.0f, "%.1f°", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(260);
        ImGui::SliderFloat("±Z rot##rrz", &scatterRotRange_[2], 0.0f, 180.0f, "%.1f°", ImGuiSliderFlags_AlwaysClamp);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.55f, 1.0f), "Scale variation (uniform ±%%)");
        ImGui::SetNextItemWidth(260);
        ImGui::SliderFloat("±Scale%%##rsc", &scatterScaleRange_, 0.0f, 100.0f, "%.1f%%", ImGuiSliderFlags_AlwaysClamp);

        ImGui::Spacing();
        ImGui::Separator();
        bool hasAnyRange = scatterPosRange_[0] != 0.0f || scatterPosRange_[1] != 0.0f ||
                           scatterPosRange_[2] != 0.0f || scatterRotRange_[0] != 0.0f ||
                           scatterRotRange_[1] != 0.0f || scatterRotRange_[2] != 0.0f ||
                           scatterScaleRange_ != 0.0f;
        bool canApply = selCount > 0 && hasAnyRange;
        if (!canApply) ImGui::BeginDisabled();
        if (ImGui::Button("Apply (new seed)", ImVec2(150, 0))) {
            randomizeTransformSelected();
        }
        if (!canApply) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !canApply)
            ImGui::SetTooltip("Select objects and set at least one non-zero range");
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Linear Array dialog
    // -----------------------------------------------------------------------
    if (arrayDupOpen_) {
        ImGui::OpenPopup("Linear Array##arrdlg");
        arrayDupOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Linear Array##arrdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        int selCount = static_cast<int>(selection_.selection().size());
        ImGui::Text("%d source object(s) selected", selCount);
        ImGui::TextDisabled("Creates copies placed along a single axis.");
        ImGui::Separator();

        // Count
        ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f), "Count (total instances)");
        ImGui::SetNextItemWidth(200);
        // AlwaysClamp (AUDIT-0044): count/spacing feed array-placement math
        // and a preview loop below; out-of-bounds values from a Ctrl+Click
        // typed entry have no other downstream guard here.
        ImGui::SliderInt("##arrcount", &arrayDupCount_, 2, 20, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::TextDisabled("(%d copies)", arrayDupCount_ - 1);

        // Axis
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Axis");
        ImGui::SameLine();
        if (ImGui::RadioButton("X", arrayDupAxis_ == 0)) arrayDupAxis_ = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Y", arrayDupAxis_ == 1)) arrayDupAxis_ = 1;
        ImGui::SameLine();
        if (ImGui::RadioButton("Z", arrayDupAxis_ == 2)) arrayDupAxis_ = 2;

        // Spacing
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.55f, 1.0f), "Spacing (world units)");
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("##arrspacing", &arrayDupSpacing_, 0.1f, -1000.0f, 1000.0f, "%.3f u", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        if (ImGui::SmallButton("=1")) arrayDupSpacing_ = 1.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("=grid")) arrayDupSpacing_ = gridSpacing_;

        // Relative toggle
        ImGui::Spacing();
        ImGui::Checkbox("Relative to source position", &arrayDupRelative_);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "ON: copy N placed at source.pos + N*spacing\n"
                "OFF: copy N placed at N*spacing (from world origin)");

        // Live preview
        if (!selection_.selection().empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            const auto& src0 = selection_.selection().front();
            float base = src0->transform.position[arrayDupAxis_];
            ImGui::TextDisabled("Preview positions on %c:", "XYZ"[arrayDupAxis_]);
            std::string preview;
            int showN = std::min(arrayDupCount_, 5);
            for (int i = 0; i < showN; ++i) {
                float pos = arrayDupRelative_
                    ? base + static_cast<float>(i) * arrayDupSpacing_
                    : static_cast<float>(i) * arrayDupSpacing_;
                char buf[24];
                std::snprintf(buf, sizeof(buf), "%.3g", pos);
                if (!preview.empty()) preview += ",  ";
                preview += buf;
            }
            if (arrayDupCount_ > 5) preview += "  …";
            ImGui::Text("  %s", preview.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        bool canApply = selCount > 0 && arrayDupCount_ >= 2;
        if (!canApply) ImGui::BeginDisabled();
        if (ImGui::Button("Create Array", ImVec2(130, 0))) {
            arrayDuplicate();
            ImGui::CloseCurrentPopup();
        }
        if (!canApply) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Group Scale dialog (H14)
    // -----------------------------------------------------------------------
    if (groupScaleOpen_) {
        ImGui::OpenPopup("Group Scale##grpscldlg");
        groupScaleOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Group Scale##grpscldlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        int selCount = static_cast<int>(selection_.selection().size());
        ImGui::Text("%d object(s) selected", selCount);
        ImGui::TextDisabled("Scales positions and sizes around the group center.");
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f), "Scale factor");
        ImGui::SetNextItemWidth(200);
        // AlwaysClamp (AUDIT-0044): canApply already guards factor > 0 before
        // enabling Apply, but clamping here keeps the displayed value sane.
        ImGui::DragFloat("##grpsclfac", &groupScaleFactor_, 0.01f, 0.01f, 100.0f, "× %.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        if (ImGui::SmallButton("×2"))   groupScaleFactor_ = 2.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("×0.5")) groupScaleFactor_ = 0.5f;
        ImGui::SameLine();
        if (ImGui::SmallButton("×1"))   groupScaleFactor_ = 1.0f;

        ImGui::TextDisabled("Moves objects and scales them proportionally.");
        ImGui::TextDisabled("Locked objects are skipped.");

        ImGui::Spacing();
        ImGui::Separator();
        bool canApply = selCount >= 2 && groupScaleFactor_ > 0.f;
        if (!canApply) ImGui::BeginDisabled();
        if (ImGui::Button("Apply", ImVec2(100, 0))) {
            groupScaleSelected();
            ImGui::CloseCurrentPopup();
        }
        if (!canApply) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Scatter Along Curve dialog (H4)
    // -----------------------------------------------------------------------
    if (scatterCurveOpen_) {
        ImGui::OpenPopup("Scatter Along Curve##scatdlg");
        scatterCurveOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Scatter Along Curve##scatdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        int selCount = static_cast<int>(selection_.selection().size());
        ImGui::Text("%d source object(s) selected", selCount);
        ImGui::Separator();

        // Count
        ImGui::TextColored(ImVec4(0.55f,1.0f,0.55f,1.0f), "Copies (total incl. original)");
        ImGui::SetNextItemWidth(200);
        // AlwaysClamp (AUDIT-0044): same reasoning as the linear-array
        // dialog above -- count/spacing/radius/angle/jitter feed placement
        // math with no other downstream guard against out-of-bounds values.
        ImGui::SliderInt("##sccount", &scatterCurveCount_, 2, 20, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::TextDisabled("(%d new)", scatterCurveCount_ - 1);

        // Mode
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f,0.85f,0.4f,1.0f), "Path type");
        ImGui::SameLine();
        if (ImGui::RadioButton("Line##scm", scatterCurveMode_ == 0)) scatterCurveMode_ = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Arc##scm",  scatterCurveMode_ == 1)) scatterCurveMode_ = 1;

        // Axis
        ImGui::TextColored(ImVec4(1.0f,0.85f,0.4f,1.0f), "Axis");
        ImGui::SameLine();
        if (ImGui::RadioButton("X##sca", scatterCurveAxis_ == 0)) scatterCurveAxis_ = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Y##sca", scatterCurveAxis_ == 1)) scatterCurveAxis_ = 1;
        ImGui::SameLine();
        if (ImGui::RadioButton("Z##sca", scatterCurveAxis_ == 2)) scatterCurveAxis_ = 2;

        ImGui::Spacing();
        if (scatterCurveMode_ == 0) {
            // Line mode
            ImGui::TextColored(ImVec4(1.0f,0.55f,0.55f,1.0f), "Spacing (units)");
            ImGui::SetNextItemWidth(200);
            ImGui::DragFloat("##scspacing", &scatterCurveSpacing_, 0.05f, -100.0f, 100.0f, "%.2f u", ImGuiSliderFlags_AlwaysClamp);
        } else {
            // Arc mode
            ImGui::TextColored(ImVec4(1.0f,0.55f,0.55f,1.0f), "Radius");
            ImGui::SetNextItemWidth(200);
            ImGui::DragFloat("##scradius", &scatterCurveRadius_, 0.05f, 0.1f, 100.0f, "%.2f u", ImGuiSliderFlags_AlwaysClamp);
            ImGui::TextColored(ImVec4(1.0f,0.55f,0.55f,1.0f), "Arc angle");
            ImGui::SetNextItemWidth(200);
            ImGui::SliderFloat("##scarc", &scatterCurveArcAngle_, 10.0f, 360.0f, "%.0f°", ImGuiSliderFlags_AlwaysClamp);
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.8f,0.8f,1.0f,1.0f), "Jitter (random offset)");
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("##scjitter", &scatterCurveJitter_, 0.01f, 0.0f, 10.0f, "%.2f u", ImGuiSliderFlags_AlwaysClamp);

        ImGui::Spacing();
        ImGui::Separator();
        bool canApply = selCount > 0 && scatterCurveCount_ >= 2;
        if (!canApply) ImGui::BeginDisabled();
        if (ImGui::Button("Scatter", ImVec2(120, 0))) {
            scatterAlongCurve();
            ImGui::CloseCurrentPopup();
        }
        if (!canApply) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Find & Replace Names dialog (Ctrl+H)
    // -----------------------------------------------------------------------
    if (findReplaceOpen_) {
        ImGui::OpenPopup("Find & Replace Names##frdlg");
        findReplaceOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Find & Replace Names##frdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        // Auto-focus the Find field on first open
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(360);
        ImGui::InputTextWithHint("##frfind", "Text to find…", findBuf_, sizeof(findBuf_));

        ImGui::SetNextItemWidth(360);
        ImGui::InputTextWithHint("##frrep", "Replace with… (empty = delete)", replaceBuf_, sizeof(replaceBuf_));

        ImGui::Spacing();
        ImGui::Checkbox("Case sensitive", &findCaseSensitive_);
        ImGui::SameLine(0, 20);
        int selCount = static_cast<int>(selection_.selection().size());
        if (selCount == 0) {
            ImGui::BeginDisabled();
            ImGui::Checkbox("Selected only", &findSelectedOnly_);
            ImGui::EndDisabled();
        } else {
            ImGui::Checkbox("Selected only", &findSelectedOnly_);
            if (findSelectedOnly_) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%d obj)", selCount);
            }
        }

        // Live preview: build a lowercase needle for case-insensitive mode
        const std::string findStr(findBuf_);
        const std::string replStr(replaceBuf_);
        std::string findLow = findStr;
        if (!findCaseSensitive_)
            for (auto& c : findLow) c = (char)std::tolower((unsigned char)c);

        auto previewReplace = [&](const std::string& src) -> std::string {
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

        if (!findStr.empty()) {
            // Walk objects and collect matches for preview
            int matchCount = 0;
            std::vector<std::pair<std::string, std::string>> previewPairs; // before → after
            std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> previewWalk;
            previewWalk = [&](const auto& list) {
                for (const auto& o : list) {
                    if (!objectLockState_.isLocked(o->id)) {
                        bool inScope = !findSelectedOnly_ || selection_.isSelected(o.get());
                        if (inScope) {
                            std::string replaced = previewReplace(o->name);
                            if (replaced != o->name) {
                                ++matchCount;
                                if (static_cast<int>(previewPairs.size()) < 4)
                                    previewPairs.push_back({ o->name, replaced });
                            }
                        }
                    }
                    previewWalk(o->children);
                }
            };
            previewWalk(document_.objects);

            ImGui::Spacing();
            ImGui::Separator();
            if (matchCount == 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No matches");
            } else {
                ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f),
                                   "%d match(es):", matchCount);
                for (const auto& [before, after] : previewPairs) {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                                       "  %s", before.c_str());
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "→");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.6f, 0.95f, 0.6f, 1.0f),
                                       "%s", after.c_str());
                }
                if (matchCount > 4)
                    ImGui::TextDisabled("  … and %d more", matchCount - 4);
            }

            ImGui::Spacing();
            bool canApply = matchCount > 0;
            if (!canApply) ImGui::BeginDisabled();
            if (ImGui::Button("Replace All", ImVec2(120, 0))) {
                findReplaceNames();
                ImGui::CloseCurrentPopup();
            }
            if (!canApply) ImGui::EndDisabled();
        } else {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Type a search term above");
            ImGui::Spacing();
            ImGui::BeginDisabled();
            ImGui::Button("Replace All", ImVec2(120, 0));
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Copy Properties to Selected dialog (Ctrl+Shift+P)
    // -----------------------------------------------------------------------
    if (copyPropsOpen_) {
        ImGui::OpenPopup("Copy Properties to Selected##cpdlg");
        copyPropsOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Copy Properties to Selected##cpdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto& sel = selection_.selection();
        if (sel.size() < 2) {
            ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Need 2+ objects selected.");
            if (ImGui::Button("Close", ImVec2(90,0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        } else {
            const auto& src = *sel.front();
            ImGui::TextDisabled("Source: "); ImGui::SameLine();
            ImGui::Text("%s", src.name.empty() ? "(unnamed)" : src.name.c_str());
            ImGui::TextDisabled("Targets: %d other object(s)", static_cast<int>(sel.size()) - 1);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Properties to copy:");
            ImGui::Spacing();
            ImGui::Checkbox("Material",          &copyPropsMaterial_);
            if (copyPropsMaterial_ && !src.material.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", src.material.c_str());
            }
            ImGui::Checkbox("Material Override", &copyPropsMaterialOverride_);
            if (copyPropsMaterialOverride_ && !src.materialOverride.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", src.materialOverride.c_str());
            }
            ImGui::Checkbox("Collision",         &copyPropsCollision_);
            if (copyPropsCollision_ && !src.collision.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", src.collision.c_str());
            }
            ImGui::Checkbox("Tags",              &copyPropsTags_);
            if (copyPropsTags_ && !src.tags.empty()) {
                std::string tagStr;
                for (const auto& t : src.tags) { if (!tagStr.empty()) tagStr += ", "; tagStr += t; }
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", tagStr.c_str());
            }
            ImGui::Checkbox("Visibility",        &copyPropsVisibility_);
            if (copyPropsVisibility_) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", src.visible ? "visible" : "hidden");
            }
            ImGui::Checkbox("Is Cutter",         &copyPropsIsCutter_);
            if (copyPropsIsCutter_) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", src.isCutter ? "yes" : "no");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool anyChecked = copyPropsMaterial_ || copyPropsMaterialOverride_ || copyPropsCollision_
                           || copyPropsTags_     || copyPropsVisibility_       || copyPropsIsCutter_;
            if (!anyChecked) ImGui::BeginDisabled();
            if (ImGui::Button("Apply", ImVec2(100, 0))) {
                copyPropsToSelected();
                ImGui::CloseCurrentPopup();
            }
            if (!anyChecked) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    // -----------------------------------------------------------------------
    // Undo History dialog
    // -----------------------------------------------------------------------
    if (undoHistoryOpen_) {
        ImGui::OpenPopup("Undo History##uhdlg");
        undoHistoryOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Undo History##uhdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        int n = undoManager_.undoCount();
        ImGui::TextDisabled("%d step(s) available  (newest first)", n);
        ImGui::Separator();
        ImGui::BeginChild("##uhscroll", ImVec2(340, std::min(n * 22 + 8, 300)), false);

        // Current state (top of stack = most recent undo point)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 1.0f, 0.55f, 1.0f));
        ImGui::Selectable("  Current  (active)", false, ImGuiSelectableFlags_Disabled);
        ImGui::PopStyleColor();

        for (int stepsAgo = 1; stepsAgo <= n; ++stepsAgo) {
            char label[64];
            std::snprintf(label, sizeof(label), "  Step -%d  (%d step%s ago)",
                          stepsAgo, stepsAgo, stepsAgo == 1 ? "" : "s");
            if (ImGui::Selectable(label)) {
                // SYS-W3-01 Phase 4: UndoManager::jumpTo() does the multi-step
                // "push current + everything newer onto redo, then restore
                // this state" bookkeeping (including the AUDIT-0056 redo-cap
                // enforcement) internally now.
                auto entry = undoManager_.jumpTo(stepsAgo, deepCopyDoc(document_), currentSelectionIds());
                if (entry) {
                    document_ = std::move(entry->doc);
                    objectIndex_.invalidate();  // SYS-W5-04: wholesale document_ replacement
                    restoreSelectionByIds(entry->selectionIds);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Box-select overlay (drawn via ImGui drawlist on top of everything)
    // -----------------------------------------------------------------------
    if (boxSelectActive_) {
        int bx0 = std::min(boxSelectX0_, boxSelectX1_);
        int by0 = std::min(boxSelectY0_, boxSelectY1_);
        int bx1 = std::max(boxSelectX0_, boxSelectX1_);
        int by1 = std::max(boxSelectY0_, boxSelectY1_);
        if (bx1 > bx0 && by1 > by0) {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            dl->AddRectFilled(ImVec2((float)bx0, (float)by0), ImVec2((float)bx1, (float)by1),
                              IM_COL32(60, 120, 200, 40));
            dl->AddRect(ImVec2((float)bx0, (float)by0), ImVec2((float)bx1, (float)by1),
                        IM_COL32(100, 160, 255, 220));
        }
    }

    // -----------------------------------------------------------------------
    // File dialog modals
    // -----------------------------------------------------------------------
    if (openDialogOpen_) {
        ImGui::OpenPopup("Open File##dlg");
        openDialogOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Open File##dlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("File path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##openpath", openDialogBuf_, sizeof(openDialogBuf_));
        if (openDialogErr_[0]) ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s", openDialogErr_);
        if (ImGui::Button("Open") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            try {
                std::filesystem::path p{openDialogBuf_};
                // SYS-W1-01 (pre-render integration point): see the matching
                // comment in MeshCraftApplication::Initialize(). SYS-W14-11:
                // dispatch (.mcb/.json/else) shared with startup + Open
                // Recent File via loadSceneFileDispatched().
                Mc3::Mc3Validation loadValidation;
                document_ = loadSceneFileDispatched(p, loadValidation);
                objectIndex_.invalidate();  // SYS-W5-04: wholesale document_ replacement
                if (!loadValidation.empty())
                    std::cout << "[MeshCraft] Load: " << loadValidation.warningCount()
                              << " warning(s), " << loadValidation.errorCount() << " error(s) in "
                              << openDialogBuf_ << "\n";
                recordValidation("Load: " + p.filename().string(), loadValidation);
                currentFile_ = openDialogBuf_;
                addRecentFile(currentFile_);
                selection_.clear();
                undoManager_.clear();
                // STAB-0250: same rationale as the "Open Recent File" path —
                // without this, a stale CSG preview cache entry from the
                // previous document could collide (same content hash) with a
                // CSG node in the newly-loaded scene and show wrong geometry.
                if (sceneRenderer_) sceneRenderer_->clearCsgCache();
                modified_ = false;
                openDialogErr_[0] = '\0';
                std::cout << "[MeshCraft] Loaded: " << openDialogBuf_ << "\n";
                setStatusMsg("Opened " + currentFile_.filename().string(), false, 2.0f);
                checkRotationConventionNotice();
                resolveImports();
                checkForNewerAutosave(currentFile_);
                updateWindowTitle();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& e) {
                std::strncpy(openDialogErr_, e.what(), sizeof(openDialogErr_) - 1);
                openDialogErr_[sizeof(openDialogErr_) - 1] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (saveDialogOpen_) {
        ImGui::OpenPopup("Save As##dlg");
        saveDialogOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Save As##dlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("File path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##savepath", saveDialogBuf_, sizeof(saveDialogBuf_));
        if (saveDialogErr_[0]) ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s", saveDialogErr_);
        if (ImGui::Button("Save") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            // AUD-031: was a hand-copied duplicate of resolveSaveAsPathAlg's
            // own path-normalization logic; now delegates to it.
            auto [path, fmt] = resolveSaveAsPathAlg(saveDialogBuf_);
            try {
                switch (fmt) {
                case SaveAsFormat::Mcb:  Mcb::saveToFile(document_, path); break;
                case SaveAsFormat::Json: document_.saveToJsonFile(path);  break;
                case SaveAsFormat::Xml:  document_.saveToFile(path);      break;
                }
                currentFile_ = path;
                addRecentFile(currentFile_);
                modified_ = false;
                autoSaveCountdown_ = 60.0f;
                { std::error_code ec; std::filesystem::remove(autoSavePath(currentFile_), ec); }
                saveDialogErr_[0] = '\0';
                std::cout << "[MeshCraft] Saved: " << path << "\n";
                setStatusMsg("Saved " + currentFile_.filename().string(), false, 2.0f);
                updateWindowTitle();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& e) {
                std::strncpy(saveDialogErr_, e.what(), sizeof(saveDialogErr_) - 1);
                saveDialogErr_[sizeof(saveDialogErr_) - 1] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Unsaved-changes confirmation dialog
    // -----------------------------------------------------------------------
    if (unsavedDlgOpen_) {
        ImGui::OpenPopup("Unsaved Changes##ucdlg");
        unsavedDlgOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Unsaved Changes##ucdlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The scene has unsaved changes.");
        ImGui::Text("Save before continuing?");
        ImGui::Spacing();
        // AUD-031: each button's should-execute-the-pending-action decision
        // now delegates to unsavedDialogResolvesToExecuteAlg (the bespoke
        // side effects around that decision -- saveFile(), the modified_/
        // pendingAction_ resets -- stay here, since the Alg mirror
        // deliberately only captures the boolean).
        if (ImGui::Button("Save", ImVec2(90, 0))) {
            bool hasFile = !currentFile_.empty();
            if (unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg::Save, hasFile)) {
                saveFile();
                executePendingAction();
            } else {
                setStatusMsg("Save the file first (Ctrl+S), then repeat the action", false, 4.0f);
                pendingAction_ = PendingAction::None;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(90, 0))) {
            modified_ = false;
            if (unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg::DontSave, !currentFile_.empty()))
                executePendingAction();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            if (!unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg::Cancel, !currentFile_.empty()))
                pendingAction_ = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Crash-recovery dialog (SYS-W9-02) -- offered when a file finishes
    // loading and its .autosave sibling is newer, a strong signal the
    // editor previously crashed/closed with unsaved changes. See
    // checkForNewerAutosave()/recoverFromAutosave()/discardAutosave() in
    // MeshCraftApplication_FileOps.cpp.
    // -----------------------------------------------------------------------
    if (recoveryDlgOpen_) {
        ImGui::OpenPopup("Recover Unsaved Changes##recoverdlg");
        recoveryDlgOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Recover Unsaved Changes##recoverdlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("An autosave for '%s' is newer than the saved file.",
                    recoveryFilePath_.filename().string().c_str());
        ImGui::Text("This usually means the editor closed or crashed with");
        ImGui::Text("unsaved changes. Recover them?");
        ImGui::Spacing();
        if (ImGui::Button("Recover", ImVec2(90, 0))) {
            recoverFromAutosave();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard Autosave", ImVec2(140, 0))) {
            discardAutosave();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep Saved File", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Keyboard Shortcuts dialog
    // -----------------------------------------------------------------------
    if (showShortcutsDialog_) {
        ImGui::OpenPopup("Keyboard Shortcuts##kbdlg");
        showShortcutsDialog_ = false;
    }
    if (ImGui::BeginPopupModal("Keyboard Shortcuts##kbdlg", nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::SetNextWindowSize(ImVec2(480, 500));
        ImGui::BeginChild("##kbscroll", ImVec2(460, 420), false, ImGuiWindowFlags_HorizontalScrollbar);

        auto kbSection = [](const char* name) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.50f, 0.85f, 1.00f, 1.f), "%s", name);
            ImGui::Separator();
        };
        auto kbRow = [](const char* key, const char* desc) {
            ImGui::TextColored(ImVec4(0.95f, 0.90f, 0.50f, 1.f), "%-26s", key);
            ImGui::SameLine();
            ImGui::Text("%s", desc);
        };

        kbSection("Command Palette");
        kbRow("Ctrl+P", "Open command palette — search commands or objects");
        kbSection("File");
        kbRow("Ctrl+N",         "New scene");
        kbRow("Ctrl+O",         "Open...");
        kbRow("Ctrl+S",         "Save");
        kbRow("Ctrl+Shift+S",   "Save As...");
        kbRow("Ctrl+E",         "Export GLB");

        kbSection("Edit");
        kbRow("Ctrl+Z / Ctrl+Y",    "Undo / Redo");
        kbRow("Ctrl+X",             "Cut");
        kbRow("Ctrl+C",             "Copy");
        kbRow("Ctrl+V",             "Paste");
        kbRow("Ctrl+D",             "Duplicate selected");
        kbRow("Ctrl+Shift+D",       "Duplicate at offset (+X by grid spacing)");
        kbRow("Del",                "Delete selected");
        kbRow("Ctrl+A",             "Select all");
        kbRow("Ctrl+I",             "Invert selection");
        kbRow("Ctrl+G",             "Group selected");
        kbRow("Ctrl+Shift+G",       "Ungroup");
        kbRow("Ctrl+H",             "Find & Replace Names");
        kbRow("Ctrl+Shift+P",       "Copy Properties to Selected");
        kbRow("Ctrl+Up / Ctrl+Down","Reorder object among siblings in hierarchy");

        kbSection("Add Object");
        kbRow("F1",  "Box");
        kbRow("F2",  "Sphere");
        kbRow("F3",  "Cylinder");
        kbRow("F4",  "Cone");
        kbRow("F5",  "Plane");

        kbSection("Viewport – Navigation");
        kbRow("Left drag",          "Orbit camera");
        kbRow("Shift + left drag",  "Pan camera");
        kbRow("Scroll wheel",       "Zoom");
        kbRow("F",                  "Focus on selection");

        kbSection("Viewport – Camera Presets");
        kbRow("Num 1",  "Front");
        kbRow("Num 3",  "Right");
        kbRow("Num 5",  "Back");
        kbRow("Num 7",  "Top");
        kbRow("Num 9",  "Bottom");

        kbSection("Viewport – Camera Bookmarks");
        kbRow("Ctrl+F1 … Ctrl+F5",  "Save camera to bookmark slot 1–5");
        kbRow("F6 … F10",           "Restore camera from bookmark slot 1–5");

        kbSection("Tools");
        kbRow("Q",  "Select");
        kbRow("W",  "Move (translate)");
        kbRow("E",  "Scale");
        kbRow("R",  "Rotate");

        kbSection("Transform Clipboard");
        kbRow("Ctrl+Shift+C", "Copy transform (position/rotation/scale) from first selected");
        kbRow("Ctrl+Shift+V", "Paste transform to all selected (skips locked; pushes undo)");
        kbRow("Ctrl+Shift+R", "Batch rename selected objects with a pattern");
        kbRow("Ctrl+H",       "Find & Replace in all object names");
        kbRow("Edit → Randomize Transform…", "Scatter selection with random pos/rot/scale offsets");

        kbSection("Transform Reset");
        kbRow("Alt+G",  "Reset position to (0, 0, 0)");
        kbRow("Alt+R",  "Reset rotation to (0°, 0°, 0°)");
        kbRow("Alt+S",  "Reset scale to (1, 1, 1)");

        kbSection("View / Visibility");
        kbRow("Alt+I",   "Isolate selection (hide all others); repeat to restore");
        kbRow("H",       "Hide selected objects");
        kbRow("Alt+H",   "Show all hidden objects");
        kbRow("Ctrl+L",  "Lock / unlock selected (blocks gizmo, nudge, delete)");
        kbRow("Alt+W",   "Toggle edge overlay");
        kbRow("Ctrl+T",  "Toggle timeline panel");

        kbSection("Hierarchy");
        kbRow("P",                    "Select parent of first selected object");
        kbRow("Double-click node",    "Rename object inline");
        kbRow("Esc (in search box)",  "Clear search filter");

        kbSection("Animation");
        kbRow("Space",  "Play / pause current action");
        kbRow("[K] (Properties panel)", "Insert keyframe at current time");

        kbSection("Other");
        kbRow("F11",  "Save screenshot");

        ImGui::EndChild();
        ImGui::Separator();
        if (ImGui::Button("Edit Bindings…", ImVec2(130, 0))) keybindOpen_ = true;
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // H9: Keybind editor dialog
    // -----------------------------------------------------------------------
    if (keybindOpen_) {
        ImGui::OpenPopup("Edit Keybindings##kbeditdlg");
        keybindOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Edit Keybindings##kbeditdlg", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::SetNextWindowSize(ImVec2(500, 560));
        ImGui::Text("Click a binding and press a new key combination to rebind.");
        ImGui::TextDisabled("Modifiers: hold Ctrl/Shift/Alt before pressing the key.");
        ImGui::Separator();

        // Ordered action list for display
        static const struct { const char* id; const char* label; } kActions[] = {
            {"file.new",          "New Scene"},
            {"file.open",         "Open..."},
            {"file.save",         "Save"},
            {"file.saveAs",       "Save As..."},
            {"file.export",       "Export GLB"},
            {"edit.undo",         "Undo"},
            {"edit.redo",         "Redo"},
            {"edit.cut",          "Cut"},
            {"edit.copy",         "Copy"},
            {"edit.paste",        "Paste"},
            {"edit.duplicate",    "Duplicate"},
            {"edit.delete",       "Delete"},
            {"edit.selectAll",    "Select All"},
            {"edit.invertSel",    "Invert Selection"},
            {"edit.group",        "Group"},
            {"edit.ungroup",      "Ungroup"},
            {"edit.batchRename",  "Batch Rename"},
            {"edit.findReplace",  "Find & Replace Names"},
            {"edit.lock",         "Lock/Unlock Selected"},
            {"view.timeline",     "Toggle Timeline"},
            {"view.hideSelected", "Hide Selected"},
            {"view.showAll",      "Show All Hidden"},
            {"view.isolate",      "Isolate Selection"},
            {"view.edgeOverlay",  "Edge Overlay"},
            {"view.focus",        "Focus Camera (F)"},
            {"tool.select",       "Tool: Select"},
            {"tool.move",         "Tool: Move"},
            {"tool.scale",        "Tool: Scale"},
            {"tool.rotate",       "Tool: Rotate"},
            {"anim.playPause",    "Play/Pause Animation"},
            {"ui.cmdPalette",     "Command Palette"},
            {"ui.screenshot",     "Save Screenshot"},
        };

        ImGui::BeginChild("##kbeditscroll", ImVec2(480, 430), false);
        for (const auto& a : kActions) {
            ImGui::PushID(a.id);
            const Editor::KeyBind& bind = keybindings_.bindings()[a.id];
            bool capturing = (keyCaptureAction_ == a.id);

            // Action label
            ImGui::TextUnformatted(a.label);
            ImGui::SameLine(220);

            // Current binding display / capture button
            std::string btnLabel = capturing ? "[Press key…]" : bind.toLabel();
            if (capturing)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.f));
            if (ImGui::SmallButton(btnLabel.c_str()))
                keyCaptureAction_ = capturing ? "" : a.id;
            if (capturing)
                ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to start capturing, press any key combo");

            // Clear button
            ImGui::SameLine();
            if (ImGui::SmallButton("×##clr")) {
                keybindings_.bindings()[a.id] = Editor::KeyBind{};
                if (keyCaptureAction_ == a.id) keyCaptureAction_.clear();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear this binding");

            ImGui::PopID();
        }
        ImGui::EndChild();

        // Key capture: check if capturing and a key was pressed
        if (!keyCaptureAction_.empty()) {
            // ImGuiKey → XNA Keys:: mapping for common keys
            static const struct { ImGuiKey imgui; int xna; } kImGuiToXna[] = {
                {ImGuiKey_A,65},{ImGuiKey_B,66},{ImGuiKey_C,67},{ImGuiKey_D,68},
                {ImGuiKey_E,69},{ImGuiKey_F,70},{ImGuiKey_G,71},{ImGuiKey_H,72},
                {ImGuiKey_I,73},{ImGuiKey_J,74},{ImGuiKey_K,75},{ImGuiKey_L,76},
                {ImGuiKey_M,77},{ImGuiKey_N,78},{ImGuiKey_O,79},{ImGuiKey_P,80},
                {ImGuiKey_Q,81},{ImGuiKey_R,82},{ImGuiKey_S,83},{ImGuiKey_T,84},
                {ImGuiKey_U,85},{ImGuiKey_V,86},{ImGuiKey_W,87},{ImGuiKey_X,88},
                {ImGuiKey_Y,89},{ImGuiKey_Z,90},
                {ImGuiKey_F1,112},{ImGuiKey_F2,113},{ImGuiKey_F3,114},{ImGuiKey_F4,115},
                {ImGuiKey_F5,116},{ImGuiKey_F6,117},{ImGuiKey_F7,118},{ImGuiKey_F8,119},
                {ImGuiKey_F9,120},{ImGuiKey_F10,121},{ImGuiKey_F11,122},{ImGuiKey_F12,123},
                {ImGuiKey_Delete,46},{ImGuiKey_Space,32},{ImGuiKey_Tab,9},
                {ImGuiKey_UpArrow,38},{ImGuiKey_DownArrow,40},
                {ImGuiKey_LeftArrow,37},{ImGuiKey_RightArrow,39},
                {ImGuiKey_PageUp,33},{ImGuiKey_PageDown,34},
                {ImGuiKey_Home,36},{ImGuiKey_End,35},{ImGuiKey_Insert,45},
                {ImGuiKey_Keypad1,97},{ImGuiKey_Keypad2,98},{ImGuiKey_Keypad3,99},
                {ImGuiKey_Keypad4,100},{ImGuiKey_Keypad5,101},{ImGuiKey_Keypad6,102},
                {ImGuiKey_Keypad7,103},{ImGuiKey_Keypad8,104},{ImGuiKey_Keypad9,105},
                {ImGuiKey_Keypad0,96},
            };
            auto& io = ImGui::GetIO();
            for (const auto& m : kImGuiToXna) {
                if (ImGui::IsKeyPressed(m.imgui, false)) {
                    Editor::KeyBind nb;
                    nb.ctrl  = io.KeyCtrl;
                    nb.shift = io.KeyShift;
                    nb.alt   = io.KeyAlt;
                    nb.key   = m.xna;
                    keybindings_.bindings()[keyCaptureAction_] = nb;
                    keyCaptureAction_.clear();
                    keybindings_.save(keybindingsPath());
                    break;
                }
            }
            // Escape cancels capture
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                keyCaptureAction_.clear();
        }

        ImGui::Separator();
        if (ImGui::Button("Reset to Defaults", ImVec2(140, 0))) {
            keybindings_.bindings().clear();
            keybindings_.initDefaults();
            keybindings_.save(keybindingsPath());
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            keyCaptureAction_.clear();
            keybindings_.save(keybindingsPath());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // H12: Macro Recorder dialog
    // -----------------------------------------------------------------------
    if (macroOpen_) {
        ImGui::OpenPopup("Macro Recorder##macrodlg");
        macroOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Macro Recorder##macrodlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        // Status indicator
        if (macroRecorder_.isRecording()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.35f, 0.35f, 1.f));
            ImGui::Text("REC  %d step(s) captured", macroRecorder_.stepCount());
            ImGui::PopStyleColor();
        } else {
            ImGui::Text("%d step(s) recorded", macroRecorder_.stepCount());
        }
        ImGui::Separator();

        // Record / Stop toggle
        if (macroRecorder_.isRecording()) {
            if (ImGui::Button("Stop", ImVec2(100, 0))) {
                macroRecorder_.stopRecording();
                setStatusMsg("Recording stopped", false, 2.f);
            }
        } else {
            if (ImGui::Button("Record", ImVec2(100, 0))) {
                macroRecorder_.startRecording();
                setStatusMsg("Recording started — edit the scene, then Stop", false, 3.f);
            }
        }
        ImGui::SameLine();

        // Play
        bool canPlay = macroRecorder_.stepCount() > 0 && !macroRecorder_.isRecording();
        if (!canPlay) ImGui::BeginDisabled();
        if (ImGui::Button("Play", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
            macroRecorder_.play(macroContext());
        }
        if (!canPlay) ImGui::EndDisabled();
        ImGui::SameLine();

        // Clear
        if (ImGui::Button("Clear", ImVec2(80, 0))) {
            macroRecorder_.clear();
        }

        // Step list
        ImGui::Separator();
        ImGui::BeginChild("##macrosteps", ImVec2(440, 180), true);
        if (macroRecorder_.stepCount() == 0) {
            ImGui::TextDisabled("(no steps)");
            ImGui::TextDisabled("Press Record, then edit the scene,");
            ImGui::TextDisabled("then Stop. Play repeats the sequence.");
        } else {
            const auto& macroSteps = macroRecorder_.steps();
            for (int i = 0; i < static_cast<int>(macroSteps.size()); ++i) {
                const auto& step = macroSteps[static_cast<size_t>(i)];
                std::string line = std::to_string(i + 1) + ". " + step.verb;
                for (const auto& a : step.args) { line += ' '; line += a; }
                ImGui::TextUnformatted(line.c_str());
            }
        }
        ImGui::EndChild();

        // Save / Load
        ImGui::Separator();
        ImGui::TextDisabled("File path (.mc3macro):");
        ImGui::SetNextItemWidth(310);
        ImGui::InputText("##macrofile", macroFileBuf_, sizeof(macroFileBuf_));
        ImGui::SameLine();
        if (ImGui::Button("Save##macrosave", ImVec2(55, 0))) macroRecorder_.save(macroFileBuf_, macroContext());
        ImGui::SameLine();
        if (ImGui::Button("Load##macroload", ImVec2(55, 0))) macroRecorder_.load(macroFileBuf_, macroContext());

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(100, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            macroRecorder_.stopRecording();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Texture drop slot-picker (D6) — shown when image dropped but no slot hovered
    // -----------------------------------------------------------------------
    if (dropTexPickerOpen_) {
        ImGui::OpenPopup("Assign Texture##texdropdlg");
        dropTexPickerOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Assign Texture##texdropdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Assign dropped texture to which slot?");
        ImGui::TextDisabled("%s", dropTexPickerPath_.c_str());
        ImGui::Spacing();
        bool hasMat = !dropTexPickerMatId_.empty() &&
                      document_.materials.count(dropTexPickerMatId_);
        if (!hasMat) {
            ImGui::TextColored(ImVec4(1.f,0.4f,0.4f,1.f), "No material selected.");
        } else {
            auto& mat = document_.materials[dropTexPickerMatId_];
            auto assign = [&](std::string& slot) {
                // F8: promote to local -- see MeshCraftApplication_UiLeftPanel.cpp's
                // "Mat" tab pushUndoMat for the full rationale.
                pushUndo();
                document_.includedMaterials.erase(dropTexPickerMatId_);
                // F9: a texture-slot field is a doc.textures KEY, not a raw
                // path -- see MeshCraftApplication.cpp's drop-consumption
                // block for the full rationale.
                slot = registerTextureFromPath(dropTexPickerPath_);
                modified_ = true;
                setStatusMsg("Texture assigned → " + dropTexPickerMatId_);
                ImGui::CloseCurrentPopup();
            };
            if (ImGui::Button("Base Color",      ImVec2(130,0))) assign(mat.baseColorTexture);
            ImGui::SameLine();
            if (ImGui::Button("Normal",          ImVec2(130,0))) assign(mat.normalTexture);
            if (ImGui::Button("Emissive",        ImVec2(130,0))) assign(mat.emissiveTexture);
            ImGui::SameLine();
            if (ImGui::Button("Metal/Roughness", ImVec2(130,0))) assign(mat.metallicRoughnessTexture);
            if (ImGui::Button("Occlusion",       ImVec2(130,0))) assign(mat.occlusionTexture);
        }
        ImGui::Spacing();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            dropTexPickerPath_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Material Export dialog (D5)
    // -----------------------------------------------------------------------
    if (matExportOpen_) {
        ImGui::OpenPopup("Export Material##matexpdlg");
        matExportOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Export Material##matexpdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Export material \"%s\" to:", matExportId_.c_str());
        ImGui::SetNextItemWidth(400);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##matexppath", matExportBuf_, sizeof(matExportBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        if (matExportErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", matExportErr_);
        ImGui::Spacing();
        bool canExp = matExportBuf_[0] != '\0' && document_.materials.count(matExportId_);
        if (!canExp) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Export", ImVec2(90, 0))) && canExp) {
            try {
                Mc3::Mc3Document tmp = exportMaterialAlg(document_, matExportId_);
                tmp.saveToFile(matExportBuf_);
                setStatusMsg("Exported material '" + matExportId_ + "' → " + matExportBuf_);
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& ex) {
                std::strncpy(matExportErr_, ex.what(), sizeof(matExportErr_)-1);
                matExportErr_[sizeof(matExportErr_)-1] = '\0';
            }
        }
        if (!canExp) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Material Import dialog (D5)
    // -----------------------------------------------------------------------
    if (matImportOpen_) {
        ImGui::OpenPopup("Import Material##matimpdlg");
        matImportOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Import Material##matimpdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Import from .mc3mat.xml file:");
        ImGui::SetNextItemWidth(400);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##matimppath", matImportBuf_, sizeof(matImportBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        if (matImportErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", matImportErr_);
        ImGui::Spacing();
        bool canImp = matImportBuf_[0] != '\0';
        if (!canImp) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Import", ImVec2(90, 0))) && canImp) {
            try {
                Mc3::Mc3Document loaded = Mc3::Mc3Document::loadFromFile(matImportBuf_);
                if (loaded.materials.empty()) {
                    std::strncpy(matImportErr_, "No materials found in file.",
                                 sizeof(matImportErr_)-1);
                } else {
                    pushUndo();
                    int added = importMaterialsAlg(document_, loaded, &selectedMaterialKey_);
                    modified_ = true; updateWindowTitle();
                    setStatusMsg("Imported " + std::to_string(added) + " material(s) from " +
                                 std::string(matImportBuf_));
                    ImGui::CloseCurrentPopup();
                }
            } catch (const std::exception& ex) {
                std::strncpy(matImportErr_, ex.what(), sizeof(matImportErr_)-1);
                matImportErr_[sizeof(matImportErr_)-1] = '\0';
            }
        }
        if (!canImp) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // GLB Export Settings dialog (F8)
    // -----------------------------------------------------------------------
    if (glbExportOpen_) {
        ImGui::OpenPopup("Export Settings##glbexpdlg");
        glbExportOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Export Settings##glbexpdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SeparatorText("Format");
        ImGui::RadioButton("GLB  (binary, self-contained)", &glbExportFmt_, 0);
        ImGui::RadioButton("GLTF (JSON + external .bin)",   &glbExportFmt_, 1);

        // Keep extension in sync when user switches format
        {
            std::string cur = glbExportOutBuf_;
            const char* wantExt = (glbExportFmt_ == 1) ? ".gltf" : ".glb";
            const char* otherExt = (glbExportFmt_ == 1) ? ".glb" : ".gltf";
            auto pos = cur.rfind(otherExt);
            if (pos != std::string::npos && pos == cur.size() - std::strlen(otherExt)) {
                cur.replace(pos, std::strlen(otherExt), wantExt);
                std::strncpy(glbExportOutBuf_, cur.c_str(), sizeof(glbExportOutBuf_)-1);
                glbExportOutBuf_[sizeof(glbExportOutBuf_)-1] = '\0';
            }
        }

        ImGui::SeparatorText("Output path");
        ImGui::SetNextItemWidth(420);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##glboutpath", glbExportOutBuf_, sizeof(glbExportOutBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SeparatorText("Options");
        ImGui::Checkbox("Allow approximate CSG export", &glbAllowApproxCSG_);
        ImGui::SameLine(); ImGui::TextDisabled("(debug fallback: disables Manifold CSG; children exported separately)");
        ImGui::BeginDisabled();
        bool embedTex = true;
        ImGui::Checkbox("Embed textures", &embedTex);
        ImGui::SameLine(); ImGui::TextDisabled("(not yet supported by mc3togltf)");
        bool quantize = false;
        ImGui::Checkbox("Quantize meshes", &quantize);
        ImGui::SameLine(); ImGui::TextDisabled("(not yet supported by mc3togltf)");
        ImGui::EndDisabled();

        if (glbExportErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", glbExportErr_);

        ImGui::Spacing();
        bool canExp = glbExportOutBuf_[0] != '\0' && !currentFile_.empty();
        if (!canExp) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Export", ImVec2(100, 0))) && canExp) {
            glbExportErr_[0] = '\0';
            try {
                runGltfExport(glbExportOutBuf_);
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& ex) {
                std::strncpy(glbExportErr_, ex.what(), sizeof(glbExportErr_)-1);
                glbExportErr_[sizeof(glbExportErr_)-1] = '\0';
            }
        }
        if (!canExp) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // OBJ Export dialog (STAB-0718)
    // -----------------------------------------------------------------------
    if (objExportOpen_) {
        ImGui::OpenPopup("Export OBJ##objexpdlg");
        objExportOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Export OBJ##objexpdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SeparatorText("Output path");
        ImGui::SetNextItemWidth(420);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##objoutpath", objExportOutBuf_, sizeof(objExportOutBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::TextDisabled("A .mtl file with the same base name is written alongside it\nif the scene has any materials.");

        if (objExportErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", objExportErr_);

        ImGui::Spacing();
        bool canExpObj = objExportOutBuf_[0] != '\0' && !currentFile_.empty();
        if (!canExpObj) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Export", ImVec2(100, 0))) && canExpObj) {
            objExportErr_[0] = '\0';
            try {
                runObjExport(objExportOutBuf_);
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& ex) {
                std::strncpy(objExportErr_, ex.what(), sizeof(objExportErr_)-1);
                objExportErr_[sizeof(objExportErr_)-1] = '\0';
            }
        }
        if (!canExpObj) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Preferences dialog (F5)
    // -----------------------------------------------------------------------
    if (prefs_.windowOpen()) {
        ImGui::OpenPopup("Preferences##prefsdlg");
        prefs_.setWindowOpen(false);
    }
    if (ImGui::BeginPopupModal("Preferences##prefsdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SeparatorText("Auto-Save");

        int asInterval = static_cast<int>(autoSaveInterval_);
        ImGui::SetNextItemWidth(160);
        // AlwaysClamp (AUDIT-0044): preferences sliders -- out-of-bounds
        // values from Ctrl+Click have no other downstream guard (autosave
        // interval, grid spacing, and snap increments all feed real math).
        if (ImGui::SliderInt("Interval (s)##asint", &asInterval, 0, 300, "%d", ImGuiSliderFlags_AlwaysClamp)) {
            autoSaveInterval_ = static_cast<float>(asInterval);
            if (autoSaveInterval_ > 0.0f && autoSaveCountdown_ > autoSaveInterval_)
                autoSaveCountdown_ = autoSaveInterval_;
        }
        ImGui::SameLine();
        if (asInterval == 0)
            ImGui::TextDisabled("(disabled)");
        else
            ImGui::Text("every %d s", asInterval);
        ImGui::SetItemTooltip("0 = auto-save disabled");

        ImGui::Spacing();
        ImGui::SeparatorText("Grid");
        ImGui::SetNextItemWidth(160);
        ImGui::SliderFloat("Cell spacing##gs", &gridSpacing_, 0.1f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

        ImGui::Spacing();
        ImGui::SeparatorText("Snap");
        ImGui::SetNextItemWidth(160);
        ImGui::SliderFloat("Translate##snt", &snapTranslate_, 0.01f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(160);
        ImGui::SliderFloat("Rotate (°)##snr", &snapRotate_, 1.0f, 90.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(160);
        ImGui::SliderFloat("Scale##sns", &snapScale_, 0.01f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

        ImGui::Spacing();
        ImGui::SeparatorText("Theme");
        {
            static const char* kThemes[] = { "Dark", "Light", "Classic" };
            for (int i = 0; i < 3; ++i) {
                if (i > 0) ImGui::SameLine();
                bool active = (prefs_.theme() == i);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
                if (ImGui::SmallButton(kThemes[i])) { prefs_.setTheme(i); prefs_.applyTheme(); }
                if (active) ImGui::PopStyleColor();
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            savePrefs();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Merge Scene dialog (F4)
    // -----------------------------------------------------------------------
    if (mergeSceneOpen_) {
        ImGui::OpenPopup("Merge Scene##mergedlg");
        mergeSceneOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Merge Scene##mergedlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Insert objects from another MC3 XML into this scene:");
        ImGui::SetNextItemWidth(420);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##mergepath", mergeSceneBuf_, sizeof(mergeSceneBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SetItemTooltip("e.g. /home/user/other.mc3.xml");
        if (mergeSceneErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", mergeSceneErr_);
        ImGui::Spacing();
        bool canMerge = mergeSceneBuf_[0] != '\0';
        if (!canMerge) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Merge", ImVec2(100, 0))) && canMerge) {
            mergeSceneErr_[0] = '\0';
            try {
                mergeSceneFromFile(mergeSceneBuf_);
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& ex) {
                std::strncpy(mergeSceneErr_, ex.what(), sizeof(mergeSceneErr_)-1);
                mergeSceneErr_[sizeof(mergeSceneErr_)-1] = '\0';
            }
        }
        if (!canMerge) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Export Selection dialog (F3)
    // -----------------------------------------------------------------------
    if (selExportOpen_) {
        ImGui::OpenPopup("Export Selection##selexpdlg");
        selExportOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Export Selection##selexpdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        int n = static_cast<int>(selection_.selection().size());
        ImGui::Text("Export %d selected object(s) to MC3 XML file:", n);
        ImGui::SetNextItemWidth(420);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##selexppath", selExportBuf_, sizeof(selExportBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SetItemTooltip("e.g. /home/user/export.mc3.xml");
        if (selExportErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", selExportErr_);
        ImGui::Spacing();
        bool canExp = selExportBuf_[0] != '\0' && !selection_.selection().empty();
        if (!canExp) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Export", ImVec2(100, 0))) && canExp) {
            selExportErr_[0] = '\0';
            try {
                exportSelectionToFile(selExportBuf_);
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& ex) {
                std::strncpy(selExportErr_, ex.what(), sizeof(selExportErr_)-1);
                selExportErr_[sizeof(selExportErr_)-1] = '\0';
            }
        }
        if (!canExp) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Mesh source browse dialog (F2)
    // -----------------------------------------------------------------------
    if (meshBrowseOpen_) {
        ImGui::OpenPopup("Mesh Source##meshbrwdlg");
        meshBrowseOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Mesh Source##meshbrwdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Path to mesh file (.obj / .glb / .gltf):");
        ImGui::SetNextItemWidth(420);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##meshbrwpath", meshBrowseBuf_, sizeof(meshBrowseBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        if (meshBrowseErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", meshBrowseErr_);
        ImGui::Spacing();
        bool canSet = meshBrowseBuf_[0] != '\0';
        if (!canSet) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Set", ImVec2(90, 0))) && canSet) {
            if (!selection_.hasSelection()) {
                std::strncpy(meshBrowseErr_, "No object selected.", sizeof(meshBrowseErr_)-1);
            } else {
                pushUndo();
                selection_.selection().front()->meshSource = meshBrowseBuf_;
                modified_ = true; updateWindowTitle();
                ImGui::CloseCurrentPopup();
            }
        }
        if (!canSet) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Import OBJ dialog (STAB-0717) — creates a new Mesh object referencing
    // the chosen file, added to the current scene (not a new-scene import).
    // -----------------------------------------------------------------------
    if (importObjDialogOpen_) {
        ImGui::OpenPopup("Import OBJ##importobjdlg");
        importObjDialogOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Import OBJ##importobjdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Path to .obj file:");
        ImGui::SetNextItemWidth(420);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##importobjpath", importObjDialogBuf_, sizeof(importObjDialogBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        if (importObjDialogErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", importObjDialogErr_);
        ImGui::Spacing();
        bool canImport = importObjDialogBuf_[0] != '\0';
        if (!canImport) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Import", ImVec2(90, 0))) && canImport) {
            std::error_code ec;
            if (!std::filesystem::exists(importObjDialogBuf_, ec) || ec) {
                std::strncpy(importObjDialogErr_, "File does not exist.", sizeof(importObjDialogErr_)-1);
            } else {
                // addPrimitive() already calls pushUndo()/adds to the scene/
                // selects the new object/sets modified_+updateWindowTitle() --
                // setting meshSource afterward on the just-selected object
                // folds into the SAME undo step (pushUndo() snapshotted the
                // document before addPrimitive() touched it at all), so no
                // separate pushUndo() call is needed here.
                addPrimitive(Mc3::ObjectType::Mesh);
                if (selection_.hasSelection())
                    selection_.selection().front()->meshSource = importObjDialogBuf_;
                setStatusMsg("Imported " + std::filesystem::path(importObjDialogBuf_).filename().string(), false, 2.0f);
                ImGui::CloseCurrentPopup();
            }
        }
        if (!canImport) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // Subtree Export as Template dialog (E8)
    // -----------------------------------------------------------------------
    if (subtreeExportOpen_) {
        ImGui::OpenPopup("Export Subtree as Template##stexpdlg");
        subtreeExportOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Export Subtree as Template##stexpdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Definition name:");
        ImGui::SetNextItemWidth(320);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputText("##stexpname", subtreeExportNameBuf_, sizeof(subtreeExportNameBuf_));

        ImGui::Text("Save to file (optional, leave blank to skip):");
        ImGui::SetNextItemWidth(320);
        ImGui::InputText("##stexpfile", subtreeExportFileBuf_, sizeof(subtreeExportFileBuf_));
        ImGui::SetItemTooltip("e.g. /home/user/templates/tree.mc3.xml");

        if (subtreeExportErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", subtreeExportErr_);

        // Warn if definition name is already in use
        bool nameUsed = subtreeExportNameBuf_[0] != '\0' &&
                        document_.definitions.count(subtreeExportNameBuf_);
        if (nameUsed)
            ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f),
                               "Warning: definition '%s' already exists and will be overwritten.",
                               subtreeExportNameBuf_);

        ImGui::Spacing();
        bool canExport = subtreeExportNameBuf_[0] != '\0' && selection_.hasSelection();
        if (!canExport) ImGui::BeginDisabled();
        if (ImGui::Button("Export", ImVec2(110, 0))) {
            subtreeExportErr_[0] = '\0';
            try {
                exportSubtreeAsTemplate(subtreeExportNameBuf_, subtreeExportFileBuf_);
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& ex) {
                std::strncpy(subtreeExportErr_, ex.what(), sizeof(subtreeExportErr_)-1);
                subtreeExportErr_[sizeof(subtreeExportErr_)-1] = '\0';
            }
        }
        if (!canExport) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------
    // K3: CSG mesh export dialog
    // -----------------------------------------------------------------------
    if (csgExportOpen_) {
        ImGui::OpenPopup("Export CSG Mesh##csgexpdlg");
        csgExportOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Export CSG Mesh##csgexpdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SeparatorText("Output path (.obj)");
        ImGui::SetNextItemWidth(420);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##csgoutpath", csgExportBuf_, sizeof(csgExportBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);

        if (csgExportErr_[0])
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", csgExportErr_);

        ImGui::Spacing();
        bool canExp = csgExportBuf_[0] != '\0' && csgExportObj_ != nullptr;
        if (!canExp) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Export", ImVec2(100, 0))) && canExp) {
            csgExportErr_[0] = '\0';
            std::string err;
            if (sceneRenderer_->exportCsgMesh(*csgExportObj_, document_, csgExportBuf_, err)) {
                setStatusMsg("Exported " +
                    std::filesystem::path(csgExportBuf_).filename().string(), false, 2.0f);
                ImGui::CloseCurrentPopup();
            } else {
                std::strncpy(csgExportErr_, err.c_str(), sizeof(csgExportErr_) - 1);
                csgExportErr_[sizeof(csgExportErr_) - 1] = '\0';
            }
        }
        if (!canExp) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    drawRegistryPanel();
    drawAiPanel();
    drawValidationPanel();
}

void MeshCraftApplication::drawPanelSplitters(int screenW, int screenH)
{
    // MeshCraft owns one application instance; keeping this ephemeral drag
    // state beside its ImGui-only implementation avoids adding fields to the
    // broadly included MeshCraftApplication header, which would otherwise
    // force an unnecessary rebuild of most editor translation units.
    static bool draggingLeft = false;
    static bool draggingRight = false;
    const float panelY  = static_cast<float>(imguiTopH_);
    const int   tlH     = showTimeline_ ? kTimelineH : 0;
    const float panelH  = static_cast<float>(screenH - imguiTopH_ - kStatusH - tlH);
    constexpr float kSplitHitWidth = 10.0f;
    constexpr float kSplitVisualWidth = 1.0f;
    const ImGuiIO& io = ImGui::GetIO();
    const float leftX = static_cast<float>(kLeftPanelW);
    const float rightX = static_cast<float>(screenW - kRightPanelW);
    const auto hitSplitter = [&](float x) {
        return io.MousePos.x >= x - kSplitHitWidth * 0.5f &&
               io.MousePos.x <= x + kSplitHitWidth * 0.5f &&
               io.MousePos.y >= panelY && io.MousePos.y < panelY + panelH;
    };
    const bool overLeft = hitSplitter(leftX);
    const bool overRight = hitSplitter(rightX);

    // The previous implementation put InvisibleButton controls into an
    // ImGuiWindowFlags_NoInputs window. Such a window deliberately rejects
    // all hit tests, so its splitters could never become active. Handle the
    // small hit areas directly instead: the viewport remains pass-through
    // everywhere except an actively dragged divider.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
        draggingLeft = overLeft;
        draggingRight = !draggingLeft && overRight;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        draggingLeft = false;
        draggingRight = false;
    }

    if (overLeft || overRight || draggingLeft || draggingRight)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    // STAB-0309: std::clamp(v, lo, hi) is undefined behavior if lo > hi.
    // A very narrow window can make screenW/2-40 less than 80, so preserve
    // the minimum before clamping either independently-sized side panel.
    const int maxPanelWidth = std::max(80, screenW / 2 - 40);
    if (draggingLeft) {
        kLeftPanelW += static_cast<int>(io.MouseDelta.x);
        kLeftPanelW = std::clamp(kLeftPanelW, 80, maxPanelWidth);
    }
    if (draggingRight) {
        kRightPanelW -= static_cast<int>(io.MouseDelta.x);
        kRightPanelW = std::clamp(kRightPanelW, 80, maxPanelWidth);
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImU32 idle = IM_COL32(120, 120, 130, 95);
    const ImU32 active = IM_COL32(245, 160, 60, 230);
    drawList->AddLine(ImVec2(leftX, panelY), ImVec2(leftX, panelY + panelH),
                      (overLeft || draggingLeft) ? active : idle, kSplitVisualWidth);
    drawList->AddLine(ImVec2(rightX, panelY), ImVec2(rightX, panelY + panelH),
                      (overRight || draggingRight) ? active : idle, kSplitVisualWidth);
}

void MeshCraftApplication::drawShadowDebugOverlay(int /*screenW*/, int screenH)
{
    if (!shadowDebugEnabled_ || !shadowDebugTextureToken_) return;

    const Mc3::Mc3Light* shadowLight = nullptr;
    for (const auto& l : document_.lights)
        if (l.type == Mc3::LightType::Directional && l.castShadows) { shadowLight = &l; break; }

    const float res = static_cast<float>(kShadowDebugRes);
    ImGui::SetNextWindowPos(ImVec2(8.f, static_cast<float>(screenH) - res - 80.f),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(res * 2.f + 20.f, res + 60.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Shadow Frustum##shadowdebug", &shadowDebugEnabled_,
                     ImGuiWindowFlags_NoScrollbar)) {
        if (shadowLight) {
            ImGui::Text("Light: %s  dir(%.2f, %.2f, %.2f)  ortho ±50 m",
                        shadowLight->name.c_str(),
                        shadowLight->direction[0],
                        shadowLight->direction[1],
                        shadowLight->direction[2]);
            ImGui::Image(static_cast<ImTextureID>(shadowDebugTextureToken_), ImVec2(res, res));
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextDisabled("Light view");
            ImGui::TextDisabled("(what the light sees)");
            ImGui::TextDisabled("Objects in this area");
            ImGui::TextDisabled("cast / receive shadows.");
            ImGui::EndGroup();
        } else {
            ImGui::TextDisabled("No directional light with Cast Shadows enabled.");
            ImGui::TextDisabled("Enable Cast Shadows on a directional light");
            ImGui::TextDisabled("in the Light properties panel.");
        }
    }
    ImGui::End();
}

} // namespace MeshCraft::Application
