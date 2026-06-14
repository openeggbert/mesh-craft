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

    // Tool buttons
    struct { ActiveTool tool; const char* label; ImVec4 col; } toolBtns[] = {
        { ActiveTool::Select,  "Select [Q]", ImVec4(0.31f,0.59f,0.82f,1.f) },
        { ActiveTool::Move,    "Move   [G]", ImVec4(0.22f,0.74f,0.39f,1.f) },
        { ActiveTool::Rotate,  "Rotate [R]", ImVec4(0.80f,0.65f,0.22f,1.f) },
        { ActiveTool::Scale,   "Scale  [S]", ImVec4(0.82f,0.29f,0.29f,1.f) },
        { ActiveTool::Measure, "Ruler",      ImVec4(0.60f,0.82f,0.82f,1.f) },
    };
    for (auto& tb : toolBtns) {
        bool active = (activeTool_ == tb.tool);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, tb.col);
        if (ImGui::Button(tb.label, ImVec2(84, 30))) { activeTool_ = tb.tool; updateWindowTitle(); }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Add primitive buttons
    struct { Mc3::ObjectType type; const char* label; ImVec4 col; } addBtns[] = {
        { Mc3::ObjectType::Box,      "+Box",  ImVec4(0.82f,0.47f,0.22f,1.f) },
        { Mc3::ObjectType::Sphere,   "+Sph",  ImVec4(0.22f,0.47f,0.82f,1.f) },
        { Mc3::ObjectType::Cylinder, "+Cyl",  ImVec4(0.22f,0.73f,0.39f,1.f) },
        { Mc3::ObjectType::Cone,     "+Con",  ImVec4(0.73f,0.22f,0.73f,1.f) },
        { Mc3::ObjectType::Plane,    "+Pln",  ImVec4(0.80f,0.80f,0.22f,1.f) },
        { Mc3::ObjectType::Torus,    "+Tor",  ImVec4(0.22f,0.70f,0.80f,1.f) },
        { Mc3::ObjectType::Capsule,  "+Cap",  ImVec4(0.70f,0.35f,0.70f,1.f) },
        { Mc3::ObjectType::Disk,     "+Dsk",  ImVec4(0.80f,0.65f,0.20f,1.f) },
        { Mc3::ObjectType::Grid,     "+Grd",  ImVec4(0.30f,0.65f,0.50f,1.f) },
        { Mc3::ObjectType::IcoSphere,"+Ico",  ImVec4(0.25f,0.55f,0.80f,1.f) },
    };
    for (auto& ab : addBtns) {
        ImGui::PushStyleColor(ImGuiCol_Button, ab.col);
        if (ImGui::Button(ab.label, ImVec2(40, 30))) addPrimitive(ab.type);
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Local/World space toggle for gizmo
    {
        const char* spaceLabel = gizmoLocalSpace_ ? "Local" : "World";
        bool wasLocal = gizmoLocalSpace_;
        if (wasLocal) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.35f, 0.70f, 1.f));
        if (ImGui::Button(spaceLabel, ImVec2(48, 30))) gizmoLocalSpace_ = !gizmoLocalSpace_;
        if (wasLocal) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gizmo space: %s  (click to toggle)", spaceLabel);
        ImGui::SameLine();
    }

    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Edge overlay toggle button
    { bool was = showEdgeOverlay_;
      if (was) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.f));
      if (ImGui::Button("Edges", ImVec2(50, 30))) showEdgeOverlay_ = !showEdgeOverlay_;
      if (was) ImGui::PopStyleColor(); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Edge overlay (wireframe lines over solid objects)");
    ImGui::SameLine();

    // Full wireframe mode toggle button
    { bool was = showWireframeMode_;
      if (was) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.30f, 1.f));
      if (ImGui::Button("Wire", ImVec2(44, 30))) showWireframeMode_ = !showWireframeMode_;
      if (was) ImGui::PopStyleColor(); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Full wireframe mode (hide solid fills, show only edges)");
    ImGui::SameLine();

    // Bounding box toggle button
    { bool was = showBoundingBox_;
      if (was) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.45f, 0.55f, 1.f));
      if (ImGui::Button("BBox", ImVec2(44, 30))) showBoundingBox_ = !showBoundingBox_;
      if (was) ImGui::PopStyleColor(); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show bounding box for selected objects");
    ImGui::SameLine();

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
            char lbl[16]; std::snprintf(lbl, sizeof(lbl), "%.2g##mt%.2g", v, v);
            if (ImGui::SmallButton(lbl)) snapTranslate_ = v;
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("##st", &snapTranslate_, 0.01f, 0.01f, 100.0f, "%.3g u");
        ImGui::Spacing();
        // Rotate presets
        ImGui::Text("Rotate (°):");
        for (float v : {5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 90.0f}) {
            bool sel = (snapRotate_ == v);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
            char lbl[16]; std::snprintf(lbl, sizeof(lbl), "%.4g°##mr%.4g", v, v);
            if (ImGui::SmallButton(lbl)) snapRotate_ = v;
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("##sr", &snapRotate_, 0.5f, 1.0f, 180.0f, "%.4g°");
        ImGui::Spacing();
        // Scale presets
        ImGui::Text("Scale:");
        for (float v : {0.05f, 0.1f, 0.25f, 0.5f, 1.0f}) {
            bool sel = (snapScale_ == v);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
            char lbl[16]; std::snprintf(lbl, sizeof(lbl), "%.2g##ms%.2g", v, v);
            if (ImGui::SmallButton(lbl)) snapScale_ = v;
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("##ss", &snapScale_, 0.01f, 0.01f, 10.0f, "%.3g");
        ImGui::EndPopup();
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
            char lbl[20]; std::snprintf(lbl, sizeof(lbl), "%.4g u##g%.4g", v, v);
            if (ImGui::Button(lbl, ImVec2(72, 0))) {
                gridSpacing_ = v;
                gridRenderer_->setSpacing(v);
            }
            if (sel) ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        ImGui::SetNextItemWidth(120);
        float tmp = gridSpacing_;
        if (ImGui::DragFloat("##gs", &tmp, 0.05f, 0.05f, 50.0f, "%.4g u")) {
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


} // namespace MeshCraft
