#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include "MeshCraft/Application/UI/Toolbar.hpp"
#include "MeshCraft/MeshCraftPrivate.hpp"

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


float MeshCraftApplication::drawToolbar(float menuBarH, int screenW)
{
    // -----------------------------------------------------------------------
    // Toolbar (below menu bar)
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(screenW), 40.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::Begin("##toolbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);

    UI::ToolbarToolsContext toolsContext{
        .activeTool = activeTool_,
        .selectTool = [this](ActiveTool tool) { activeTool_ = tool; updateWindowTitle(); },
        .addPrimitive = [this](Mc3::ObjectType type) { addPrimitive(type); },
    };
    UI::Toolbar::drawTools(toolsContext);

    UI::Toolbar::drawDisplayToggles(gizmoLocalSpace_, showEdgeOverlay_,
                                    showWireframeMode_, showBoundingBox_);

    // Snap-to-grid toggle button (left-click toggles, right-click configures)
    { bool was = snapEnabled_;
      if (was) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
      if (ImGui::Button("Snap", ImVec2(44, 30))) snapEnabled_ = !snapEnabled_;
      if (was) ImGui::PopStyleColor(); }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Snap to grid  Move: %.2g u  Rotate: %.0f°  Scale: %.2g\nRight-click to configure",
                          snapTranslate_, snapRotate_, snapScale_);
    if (ImGui::BeginPopupContextItem("##snapcfg")) {
        ImGui::TextDisabled("Snap Intervals");
        ImGui::Separator();
        // Move presets
        ImGui::Text("Move (u):");
        for (float v : {0.1f, 0.25f, 0.5f, 1.0f, 2.0f}) {
            bool sel = (snapTranslate_ == v);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
            // AUD-054: buffer widened from 16 -- gcc's format-truncation
            // analysis can't prove %g of an arbitrary float always stays
            // short (it does for this loop's actual value set, but not for
            // float's full range), so it flagged a theoretical truncation.
            char lbl[32]; std::snprintf(lbl, sizeof(lbl), "%.2g##mt%.2g", v, v);
            if (ImGui::SmallButton(lbl)) snapTranslate_ = v;
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::SetNextItemWidth(120);
        // AlwaysClamp (AUDIT-0046): without it, Ctrl+Click text entry can set
        // these outside their slider bounds (incl. zero/negative), which
        // would break snap-increment/grid/proportional-edit math downstream.
        ImGui::DragFloat("##st", &snapTranslate_, 0.01f, 0.01f, 100.0f, "%.3g u", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Spacing();
        // Rotate presets
        ImGui::Text("Rotate (°):");
        for (float v : {5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 90.0f}) {
            bool sel = (snapRotate_ == v);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
            char lbl[32]; std::snprintf(lbl, sizeof(lbl), "%.4g°##mr%.4g", v, v);
            if (ImGui::SmallButton(lbl)) snapRotate_ = v;
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("##sr", &snapRotate_, 0.5f, 1.0f, 180.0f, "%.4g°", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Spacing();
        // Scale presets
        ImGui::Text("Scale:");
        for (float v : {0.05f, 0.1f, 0.25f, 0.5f, 1.0f}) {
            bool sel = (snapScale_ == v);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
            char lbl[32]; std::snprintf(lbl, sizeof(lbl), "%.2g##ms%.2g", v, v);
            if (ImGui::SmallButton(lbl)) snapScale_ = v;
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("##ss", &snapScale_, 0.01f, 0.01f, 10.0f, "%.3g", ImGuiSliderFlags_AlwaysClamp);
        ImGui::EndPopup();
    }
    ImGui::SameLine();

    // Surface snap toggle (B8)
    { bool was = surfaceSnapEnabled_;
      if (was) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
      if (ImGui::Button("Surf", ImVec2(40, 30))) surfaceSnapEnabled_ = !surfaceSnapEnabled_;
      if (was) ImGui::PopStyleColor(); }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Surface snap: snap Y to the top of the surface below the object");
    ImGui::SameLine();

    // Proportional editing toggle + radius (H1)
    { bool was = propEditEnabled_;
      if (was) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.20f, 0.45f, 1.f));
      if (ImGui::Button("Prop", ImVec2(40, 30))) propEditEnabled_ = !propEditEnabled_;
      if (was) ImGui::PopStyleColor(); }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Proportional editing: Move influences nearby objects (Gaussian falloff)\nRadius: %.2f u", propEditRadius_);
    if (propEditEnabled_) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        // AlwaysClamp (AUDIT-0046): same Ctrl+Click out-of-bounds risk; a
        // zero/negative proportional-edit radius would break the Gaussian
        // falloff math with no other downstream guard.
        ImGui::SliderFloat("##propR", &propEditRadius_, 0.5f, 50.0f, "R:%.1f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Proportional edit radius (world units)");
    }
    ImGui::SameLine();

    // Grid cell size button (right-click to configure)
    if (ImGui::Button("Grid", ImVec2(40, 30))) {}
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Grid cell size: %.4g u\nRight-click to change", gridSpacing_);
    if (ImGui::BeginPopupContextItem("##gridcfg")) {
        ImGui::TextDisabled("Grid Cell Size");
        ImGui::Separator();
        for (float v : {0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f}) {
            bool sel = (gridSpacing_ == v);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
            char lbl[32]; std::snprintf(lbl, sizeof(lbl), "%.4g u##g%.4g", v, v);
            if (ImGui::Button(lbl, ImVec2(72, 0))) {
                gridSpacing_ = v;
                gridRenderer_->setSpacing(v);
            }
            if (sel) ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        ImGui::SetNextItemWidth(120);
        float tmp = gridSpacing_;
        // AlwaysClamp (AUDIT-0046): a zero/negative grid spacing would break
        // GridRenderer::setSpacing() with no other downstream guard.
        if (ImGui::DragFloat("##gs", &tmp, 0.05f, 0.05f, 50.0f, "%.4g u", ImGuiSliderFlags_AlwaysClamp)) {
            gridSpacing_ = tmp;
            gridRenderer_->setSpacing(tmp);
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();

    float toolbarH = ImGui::GetWindowHeight();
    imguiTopH_ = static_cast<int>(menuBarH + toolbarH);

    ImGui::End();
    ImGui::PopStyleVar(2);
    return toolbarH;
}


} // namespace MeshCraft::Application

namespace MeshCraft::Application::UI {

void Toolbar::drawTools(ToolbarToolsContext& context) {
    struct { ActiveTool tool; const char* label; ImVec4 color; } tools[] = {
        {ActiveTool::Select, "Select [Q]", {0.31f, 0.59f, 0.82f, 1.f}},
        {ActiveTool::Move, "Move   [G]", {0.22f, 0.74f, 0.39f, 1.f}},
        {ActiveTool::Rotate, "Rotate [R]", {0.80f, 0.65f, 0.22f, 1.f}},
        {ActiveTool::Scale, "Scale  [S]", {0.82f, 0.29f, 0.29f, 1.f}},
        {ActiveTool::Measure, "Ruler", {0.60f, 0.82f, 0.82f, 1.f}},
    };
    for (const auto& tool : tools) {
        const bool active = context.activeTool == tool.tool;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, tool.color);
        if (ImGui::Button(tool.label, ImVec2(84, 30))) context.selectTool(tool.tool);
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    struct { Mc3::ObjectType type; const char* label; ImVec4 color; } primitives[] = {
        {Mc3::ObjectType::Box, "+Box", {0.82f, 0.47f, 0.22f, 1.f}},
        {Mc3::ObjectType::Sphere, "+Sph", {0.22f, 0.47f, 0.82f, 1.f}},
        {Mc3::ObjectType::Cylinder, "+Cyl", {0.22f, 0.73f, 0.39f, 1.f}},
        {Mc3::ObjectType::Cone, "+Con", {0.73f, 0.22f, 0.73f, 1.f}},
        {Mc3::ObjectType::Plane, "+Pln", {0.80f, 0.80f, 0.22f, 1.f}},
        {Mc3::ObjectType::Torus, "+Tor", {0.22f, 0.70f, 0.80f, 1.f}},
        {Mc3::ObjectType::Capsule, "+Cap", {0.70f, 0.35f, 0.70f, 1.f}},
        {Mc3::ObjectType::Disk, "+Dsk", {0.80f, 0.65f, 0.20f, 1.f}},
        {Mc3::ObjectType::Grid, "+Grd", {0.30f, 0.65f, 0.50f, 1.f}},
        {Mc3::ObjectType::IcoSphere, "+Ico", {0.25f, 0.55f, 0.80f, 1.f}},
    };
    for (const auto& primitive : primitives) {
        ImGui::PushStyleColor(ImGuiCol_Button, primitive.color);
        if (ImGui::Button(primitive.label, ImVec2(40, 30))) context.addPrimitive(primitive.type);
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::TextDisabled("|");
    ImGui::SameLine();
}

void Toolbar::drawDisplayToggles(bool& gizmoLocalSpace, bool& showEdges,
                                 bool& showWireframe, bool& showBoundingBox) {
    const char* spaceLabel = gizmoLocalSpace ? "Local" : "World";
    const bool wasLocal = gizmoLocalSpace;
    if (wasLocal) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.35f, 0.70f, 1.f));
    if (ImGui::Button(spaceLabel, ImVec2(48, 30))) gizmoLocalSpace = !gizmoLocalSpace;
    if (wasLocal) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gizmo space: %s  (click to toggle)", spaceLabel);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    const auto toggle = [](const char* label, float width, ImVec4 color, bool& value, const char* tooltip) {
        const bool wasEnabled = value;
        if (wasEnabled) ImGui::PushStyleColor(ImGuiCol_Button, color);
        if (ImGui::Button(label, ImVec2(width, 30))) value = !value;
        if (wasEnabled) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        ImGui::SameLine();
    };
    toggle("Edges", 50, ImVec4(0.20f, 0.20f, 0.20f, 1.f), showEdges,
           "Edge overlay (wireframe lines over solid objects)");
    toggle("Wire", 44, ImVec4(0.15f, 0.55f, 0.30f, 1.f), showWireframe,
           "Full wireframe mode (hide solid fills, show only edges)");
    toggle("BBox", 44, ImVec4(0.10f, 0.45f, 0.55f, 1.f), showBoundingBox,
           "Show bounding box for selected objects");
}

} // namespace MeshCraft::Application::UI
