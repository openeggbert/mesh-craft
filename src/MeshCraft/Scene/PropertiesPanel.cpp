#include "MeshCraft/Scene/PropertiesPanel.hpp"
#include "MeshCraft/Renderer/SceneRenderer.hpp"
#include "MeshCraft/EditorCommandAlgorithms.hpp"

#include <imgui.h>

#include <CNA/Devices/FileDialog.hpp>
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

namespace MeshCraft::Scene {

using namespace Microsoft::Xna::Framework;

// STAB-0712: Mc3Primitive::axis (the mesh-generation axis hint consumed by
// buildCylinder()/buildPlane()/buildCapsule()/buildDisk() in
// mc3togltf/src/MeshBuilder.cpp) had no UI anywhere -- shared by the 4
// primitive types that actually use it, matching this file's existing
// per-case-block editing style.
static void drawAxisCombo(const PropertiesContext& ctx, Mc3::Mc3Primitive& p, const char* widgetId)
{
    const char* axisOpts[] = { "x", "y", "z" };
    int axisIdx = 1; // default "y"
    for (int i = 0; i < 3; ++i)
        if (p.axis == axisOpts[i]) { axisIdx = i; break; }
    ImGui::TextDisabled("Axis");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo(widgetId, &axisIdx, axisOpts, 3)) {
        ctx.pushUndo();
        p.axis = axisOpts[axisIdx];
        ctx.markModified();
    }
}

static const char* simpleWalkCollisionType(const Mc3::Mc3Object& obj)
{
    if (!obj.primitive) return nullptr;
    switch (obj.primitive->primitiveType) {
    case Mc3::PrimitiveType::Box:
    case Mc3::PrimitiveType::Cube:
        return "box";
    case Mc3::PrimitiveType::Sphere:
    case Mc3::PrimitiveType::IcoSphere:
        return "sphere";
    case Mc3::PrimitiveType::Capsule:
        return "capsule";
    default:
        return nullptr;
    }
}

void PropertiesPanel::draw(float panelX, float panelY, float panelW, float panelH,
                           const PropertiesContext& ctx)
{
    ImGui::SetNextWindowPos(ImVec2(panelX, panelY));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.155f, 0.155f, 0.155f, 1.0f));
    ImGui::Begin("Properties", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    if (ctx.selection.hasSelection()) {
        auto& sel0 = ctx.selection.selection().front();

        // Object name
        {
            char nameBuf[128];
            std::strncpy(nameBuf, sel0->name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf)-1] = '\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                ctx.pushUndo();
                std::string oldName = sel0->name;
                sel0->name = nameBuf;
                renameObjectInActionsAlg(ctx.document.actions, oldName, sel0->name);
                ctx.markModified();
            }
        }

        ImGui::Spacing();

        // Multi-select helpers
        int selN = static_cast<int>(ctx.selection.selection().size());
        const auto& selAll = ctx.selection.selection();

        auto allMatchF3 = [&](auto getter) -> bool {
            if (selN <= 1) return true;
            auto ref = getter(sel0.get());
            for (const auto& s : selAll) {
                auto v = getter(s.get());
                if (v[0] != ref[0] || v[1] != ref[1] || v[2] != ref[2]) return false;
            }
            return true;
        };
        auto allMatchStr = [&](auto getter) -> bool {
            if (selN <= 1) return true;
            const auto ref = getter(sel0.get());
            for (const auto& s : selAll) if (getter(s.get()) != ref) return false;
            return true;
        };
        auto allMatchBool = [&](auto getter) -> bool {
            if (selN <= 1) return true;
            bool ref = getter(sel0.get());
            for (const auto& s : selAll) if (getter(s.get()) != ref) return false;
            return true;
        };

        auto multiLabel = [&](const char* label, bool mixed = false) {
            ImGui::TextDisabled("%s", label);
            if (selN > 1) {
                ImGui::SameLine();
                char badge[16]; std::snprintf(badge, sizeof(badge), "(+%d)", selN - 1);
                ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 0.8f), "%s", badge);
            }
            if (mixed) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "~");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Values differ across selection");
            }
        };

        if (ImGui::BeginTabBar("##proptabs")) {

        if (ImGui::BeginTabItem("Transform")) {

        // Transform: position (delta applied to all selected, skipping locked)
        multiLabel("Position", !allMatchF3([](const Mc3::Mc3Object* o){ return o->transform.position; }));
        if (ctx.showTimeline && !ctx.currentActionName.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("K##kpos"))
                ctx.insertKeyframes(*sel0, {Mc3::AnimatedProperty::PositionX,
                                            Mc3::AnimatedProperty::PositionY,
                                            Mc3::AnimatedProperty::PositionZ});
        }
        {
            float pos[3] = { sel0->transform.position[0], sel0->transform.position[1], sel0->transform.position[2] };
            ImGui::SetNextItemWidth(-1);
            { if (ctx.undoOnActivate(ImGui::DragFloat3("##pos", pos, 0.1f))) {
                float dp[3] = { pos[0]-sel0->transform.position[0],
                                 pos[1]-sel0->transform.position[1],
                                 pos[2]-sel0->transform.position[2] };
                for (const auto& s : ctx.selection.selection()) {
                    if (ctx.lockedIds.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i) s->transform.position[i] += dp[i];
                }
                ctx.markModified();
            } }
            // World-space position (read-only, shown when object is parented)
            {
                auto wm = ctx.renderer->computeObjectWorldMatrix(*sel0, ctx.document);
                ImGui::TextDisabled("World: %.3f, %.3f, %.3f", wm.M41, wm.M42, wm.M43);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("World-space position (after parent transforms)");
            }
        }

        // Transform: rotation (delta applied to all selected)
        multiLabel("Rotation", !allMatchF3([](const Mc3::Mc3Object* o){ return o->transform.rotation; }));
        if (ctx.showTimeline && !ctx.currentActionName.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("K##krot"))
                ctx.insertKeyframes(*sel0, {Mc3::AnimatedProperty::RotationX,
                                            Mc3::AnimatedProperty::RotationY,
                                            Mc3::AnimatedProperty::RotationZ});
        }
        {
            float rot[3] = { sel0->transform.rotation[0], sel0->transform.rotation[1], sel0->transform.rotation[2] };
            ImGui::SetNextItemWidth(-1);
            { if (ctx.undoOnActivate(ImGui::DragFloat3("##rot", rot, 0.5f))) {
                float dr[3] = { rot[0]-sel0->transform.rotation[0],
                                 rot[1]-sel0->transform.rotation[1],
                                 rot[2]-sel0->transform.rotation[2] };
                for (const auto& s : ctx.selection.selection()) {
                    if (ctx.lockedIds.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i) s->transform.rotation[i] += dr[i];
                }
                ctx.markModified();
            } }
        }

        // Transform: scale (delta applied to all selected)
        multiLabel("Scale", !allMatchF3([](const Mc3::Mc3Object* o){ return o->transform.scale; }));
        if (ctx.showTimeline && !ctx.currentActionName.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("K##kscl"))
                ctx.insertKeyframes(*sel0, {Mc3::AnimatedProperty::ScaleX,
                                            Mc3::AnimatedProperty::ScaleY,
                                            Mc3::AnimatedProperty::ScaleZ});
        }
        {
            float scl[3] = { sel0->transform.scale[0], sel0->transform.scale[1], sel0->transform.scale[2] };
            ImGui::SetNextItemWidth(-1);
            // AlwaysClamp (AUDIT-0042): bounded transform/primitive/material
            // params in this file have no other downstream guard against an
            // out-of-range Ctrl+Click-typed value; unbounded ones
            // (position/rotation/pivot/path-point DragFloat3 calls with no
            // explicit min/max) are deliberately left untouched.
            { if (ctx.undoOnActivate(ImGui::DragFloat3("##scl", scl, 0.01f, 0.001f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))) {
                float ds[3] = { scl[0]-sel0->transform.scale[0],
                                 scl[1]-sel0->transform.scale[1],
                                 scl[2]-sel0->transform.scale[2] };
                for (const auto& s : ctx.selection.selection()) {
                    if (ctx.lockedIds.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i)
                        s->transform.scale[i] = std::max(0.001f, s->transform.scale[i] + ds[i]);
                }
                ctx.markModified();
            } }
        }

        // Computed world-space size (read-only display)
        {
            const float sx = std::abs(sel0->transform.scale[0]);
            const float sy = std::abs(sel0->transform.scale[1]);
            const float sz = std::abs(sel0->transform.scale[2]);
            float w = 0, h = 0, d = 0;
            bool sizeKnown = false;
            if (sel0->primitive.has_value()) {
                const auto& p = *sel0->primitive;
                switch (p.primitiveType) {
                    case Mc3::PrimitiveType::Box:
                    case Mc3::PrimitiveType::Cube:
                        w = p.size[0]*sx; h = p.size[1]*sy; d = p.size[2]*sz;
                        sizeKnown = true; break;
                    case Mc3::PrimitiveType::Sphere:
                        w = h = d = p.radius*2.0f*std::max({sx,sy,sz});
                        sizeKnown = true; break;
                    case Mc3::PrimitiveType::Cylinder:
                    case Mc3::PrimitiveType::Cone:
                        w = p.radius*2.0f*std::max(sx,sz);
                        h = p.height*sy;
                        d = w;
                        sizeKnown = true; break;
                    case Mc3::PrimitiveType::Plane:
                        w = p.size[0]*sx; h = 0.0f; d = p.size[2]*sz;
                        sizeKnown = true; break;
                    case Mc3::PrimitiveType::Torus:
                        w = d = (p.majorRadius + p.minorRadius) * 2.0f * std::max(sx,sz);
                        h = p.minorRadius * 2.0f * sy;
                        sizeKnown = true; break;
                    case Mc3::PrimitiveType::Capsule:
                        w = d = p.radius * 2.0f * std::max(sx,sz);
                        h = (p.height + p.radius * 2.0f) * sy;
                        sizeKnown = true; break;
                    case Mc3::PrimitiveType::Disk:
                        w = d = p.radius * 2.0f * std::max(sx,sz);
                        h = 0.0f;
                        sizeKnown = true; break;
                    case Mc3::PrimitiveType::Grid:
                        w = p.size[0]*sx; h = 0.0f; d = p.size[2]*sz;
                        sizeKnown = true; break;
                    case Mc3::PrimitiveType::IcoSphere:
                        w = h = d = p.radius * 2.0f * std::max({sx,sy,sz});
                        sizeKnown = true; break;
                    default: break;
                }
            }
            if (sizeKnown) {
                char szBuf[64];
                std::snprintf(szBuf, sizeof(szBuf), "%.4g \xc3\x97 %.4g \xc3\x97 %.4g u", w, h, d);
                ImGui::TextDisabled("Size");
                ImGui::SameLine();
                ImGui::TextDisabled("%s", szBuf);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("World-space W \xc3\x97 H \xc3\x97 D (scale applied)");
            } else if (ctx.selection.selection().size() >= 2) {
                // Multi-selection: span of pivot positions
                float mn[3] = { 1e30f,  1e30f,  1e30f};
                float mx[3] = {-1e30f, -1e30f, -1e30f};
                for (const auto& s : ctx.selection.selection()) {
                    for (int i = 0; i < 3; ++i) {
                        mn[i] = std::min(mn[i], s->transform.position[i]);
                        mx[i] = std::max(mx[i], s->transform.position[i]);
                    }
                }
                char szBuf[64];
                std::snprintf(szBuf, sizeof(szBuf), "%.4g \xc3\x97 %.4g \xc3\x97 %.4g u",
                              mx[0]-mn[0], mx[1]-mn[1], mx[2]-mn[2]);
                ImGui::TextDisabled("Span");
                ImGui::SameLine();
                ImGui::TextDisabled("%s", szBuf);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Bounding span of selected pivot positions");
            }
        }

        // Transform: pivot
        {
            ImVec4 btnCol = ctx.pivotEditMode
                ? ImVec4(0.9f,0.5f,0.1f,1.f)
                : ImGui::GetStyleColorVec4(ImGuiCol_Button);
            ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
            if (ImGui::Button(ctx.pivotEditMode ? "Pivot (ON)" : "Pivot", ImVec2(-1,0))) {
                ctx.pivotEditMode = !ctx.pivotEditMode;
                if (ctx.pivotEditMode) ctx.setPivotMoveTool();
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Toggle Move-Pivot mode: gizmo moves the pivot\nwithout moving the geometry");
        }
        multiLabel("Pivot", !allMatchF3([](const Mc3::Mc3Object* o){ return o->transform.pivot; }));
        {
            float piv[3] = { sel0->transform.pivot[0], sel0->transform.pivot[1], sel0->transform.pivot[2] };
            ImGui::SetNextItemWidth(-60);
            { bool _undoCh305 = ImGui::DragFloat3("##piv", piv, 0.1f);
            if (ImGui::IsItemActivated()) ctx.pushUndo();
            if (_undoCh305) {
                float dp[3] = { piv[0]-sel0->transform.pivot[0],
                                 piv[1]-sel0->transform.pivot[1],
                                 piv[2]-sel0->transform.pivot[2] };
                for (const auto& s : selAll) {
                    if (ctx.lockedIds.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i) s->transform.pivot[i] += dp[i];
                }
                ctx.markModified();
            } }
            ImGui::SameLine();
            if (ImGui::Button("Reset##piv", ImVec2(-1,0))) {
                ctx.resetPivot();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Zero the pivot and compensate position\nso geometry stays in place");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Visible
        {
            bool visMixed = !allMatchBool([](const Mc3::Mc3Object* o){ return o->visible; });
            bool vis = sel0->visible;
            if (ImGui::Checkbox("Visible", &vis)) {
                ctx.pushUndo();
                for (const auto& s : selAll) s->visible = vis;
                ctx.markModified();
            }
            if (visMixed) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f,0.75f,0.2f,1.0f),"~");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Values differ across selection");
            }
            if (ctx.showTimeline && !ctx.currentActionName.empty()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("K##kvis"))
                    ctx.insertKeyframes(*sel0, {Mc3::AnimatedProperty::Visible});
            }
        }

        // Material assignment
        {
            bool matTopMixed = !allMatchStr([](const Mc3::Mc3Object* o){ return o->material; });
            ImGui::TextDisabled("Material");
            if (matTopMixed) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f,0.75f,0.2f,1.0f),"~"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("Values differ across selection"); }
            // Build list: first entry is "(none)"
            std::vector<std::string> matKeys;
            matKeys.push_back("(none)");
            for (const auto& [k, _] : ctx.document.materials) matKeys.push_back(k);
            int curIdx = 0;
            for (int i = 1; i < (int)matKeys.size(); ++i)
                if (matKeys[i] == sel0->material) { curIdx = i; break; }
            ImGui::SetNextItemWidth(-1);
            const char* matTopPreview = matTopMixed ? "(mixed)" : (curIdx == 0 ? "(none)" : sel0->material.c_str());
            if (ImGui::BeginCombo("##matsel0", matTopPreview)) {
                for (int i = 0; i < (int)matKeys.size(); ++i) {
                    bool isSel = (!matTopMixed && i == curIdx);
                    if (i > 0 && ctx.document.materials.count(matKeys[i])) {
                        const auto& m = ctx.document.materials.at(matKeys[i]);
                        ImVec4 c(m.baseColor[0], m.baseColor[1], m.baseColor[2], 1.0f);
                        ImGui::ColorButton("##mcb", c,
                            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                            ImVec2(12, 12));
                        ImGui::SameLine(0, 4);
                    }
                    if (ImGui::Selectable(matKeys[i].c_str(), isSel)) {
                        ctx.pushUndo();
                        const std::string newMat = (i == 0) ? "" : matKeys[i];
                        for (const auto& s : selAll) s->material = newMat;
                        ctx.markModified();
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ctx.document.materials.empty())
                ImGui::TextDisabled("(define materials in the Mat tab)");
        }

        // SYS-W14-10: Mc3Object::scriptId attachment. Scripts themselves are
        // authored in the left panel's "Scripts" tab (doc.scripts); this is
        // just the reference from an object to one of them (R103 -- the
        // script's actual execution is left to each consumer, not run by
        // this editor). Mirrors the Material combo above (a doc-level
        // std::map<std::string,...> resolved by key), minus the color swatch.
        {
            bool scriptTopMixed = !allMatchStr([](const Mc3::Mc3Object* o){ return o->scriptId; });
            ImGui::TextDisabled("Script");
            if (scriptTopMixed) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f,0.75f,0.2f,1.0f),"~"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("Values differ across selection"); }
            std::vector<std::string> scriptKeys;
            scriptKeys.push_back("(none)");
            for (const auto& [k, _] : ctx.document.scripts) scriptKeys.push_back(k);
            int scriptCurIdx = 0;
            for (int i = 1; i < (int)scriptKeys.size(); ++i)
                if (scriptKeys[i] == sel0->scriptId) { scriptCurIdx = i; break; }
            ImGui::SetNextItemWidth(-1);
            const char* scriptPreview = scriptTopMixed ? "(mixed)" : (scriptCurIdx == 0 ? "(none)" : sel0->scriptId.c_str());
            if (ImGui::BeginCombo("##scriptsel0", scriptPreview)) {
                for (int i = 0; i < (int)scriptKeys.size(); ++i) {
                    bool isSel = (!scriptTopMixed && i == scriptCurIdx);
                    std::string label = scriptKeys[i];
                    if (i > 0 && ctx.document.scripts.count(scriptKeys[i]))
                        label += " (" + ctx.document.scripts.at(scriptKeys[i]).type + ")";
                    if (ImGui::Selectable(label.c_str(), isSel)) {
                        ctx.pushUndo();
                        const std::string newScript = (i == 0) ? "" : scriptKeys[i];
                        for (const auto& s : selAll) s->scriptId = newScript;
                        ctx.markModified();
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ctx.document.scripts.empty())
                ImGui::TextDisabled("(define scripts in the Scripts tab)");
        }

        // Collision
        {
            bool colMixed = !allMatchStr([](const Mc3::Mc3Object* o){ return o->collision; });
            ImGui::TextDisabled("Collision");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Walk Mode supports box, sphere, and vertical capsule proxies.\n"
                                  "mesh/convex and incompatible transforms are visibly ignored, never approximated.");
            if (colMixed) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f,0.75f,0.2f,1.0f),"~"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("Values differ across selection"); }
            const char* colOpts[] = { "none", "box", "sphere", "mesh", "convex", "capsule" };
            int colIdx = 0;
            for (int i = 0; i < 6; ++i)
                if (sel0->collision == colOpts[i]) { colIdx = i; break; }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##col", &colIdx, colOpts, 6)) {
                ctx.pushUndo();
                for (const auto& s : selAll) s->collision = colOpts[colIdx];
                ctx.markModified();
            }
            bool canGenerateSimpleProxy = false;
            for (const auto& s : selAll) {
                if (!ctx.lockedIds.count(s->id) && simpleWalkCollisionType(*s)) {
                    canGenerateSimpleProxy = true;
                    break;
                }
            }
            if (!canGenerateSimpleProxy) ImGui::BeginDisabled();
            if (ImGui::Button("Generate Simple Proxy", ImVec2(-1, 0)))
                ctx.generateSimpleCollisionProxy();
            if (!canGenerateSimpleProxy) ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Assign box to Box/Cube, sphere to Sphere/IcoSphere, or capsule to Capsule.\n"
                                  "Only unlocked supported primitives are changed.");
        }

        // Layer (E7)
        {
            bool layerMixed = !allMatchStr([](const Mc3::Mc3Object* o){ return o->layer; });
            ImGui::TextDisabled("Layer");
            if (layerMixed) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f,0.75f,0.2f,1.0f),"~"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("Values differ across selection"); }
            static char layerBuf[64]{};
            if (!layerMixed) {
                std::strncpy(layerBuf, sel0->layer.c_str(), sizeof(layerBuf) - 1);
                layerBuf[sizeof(layerBuf) - 1] = '\0';
            } else {
                layerBuf[0] = '\0';
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##layer", layerBuf, sizeof(layerBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                ctx.pushUndo();
                for (const auto& s : selAll) s->layer = layerBuf;
                ctx.markModified();
            }
            ImGui::SetItemTooltip("Named layer (press Enter to apply). Empty = default layer.");
        }

        // Tags — chip display with per-chip remove button + typed-add field
        {
            ImGui::TextDisabled("Tags");

            int tagToRemove = -1;
            for (int ti = 0; ti < static_cast<int>(sel0->tags.size()); ++ti) {
                ImGui::PushID(ti);
                ImGui::PushStyleColor(ImGuiCol_Button,
                    ImVec4(0.18f, 0.38f, 0.62f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    ImVec4(0.55f, 0.30f, 0.30f, 1.0f));
                if (ImGui::SmallButton("x")) tagToRemove = ti;
                ImGui::PopStyleColor(2);
                ImGui::SameLine(0, 3);
                ImGui::TextColored(ImVec4(0.65f, 0.88f, 1.0f, 1.0f),
                                   "%s", sel0->tags[ti].c_str());
                ImGui::PopID();
            }
            if (tagToRemove >= 0) {
                ctx.pushUndo();
                sel0->tags.erase(sel0->tags.begin() + tagToRemove);
                ctx.markModified();
            }

            // Add-new-tag input (Enter or + button)
            static char newTagBuf[64]{};
            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetNextItemWidth(avail - 28.0f);
            bool addTag = ImGui::InputTextWithHint("##newtag", "add tag…",
                              newTagBuf, sizeof(newTagBuf),
                              ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine(0, 3);
            addTag |= ImGui::SmallButton("+##addtag");
            if (addTag && newTagBuf[0] != '\0') {
                std::string t(newTagBuf);
                auto s = t.find_first_not_of(" \t");
                auto e = t.find_last_not_of(" \t");
                if (s != std::string::npos) {
                    std::string trimmed = t.substr(s, e - s + 1);
                    bool exists = std::find(sel0->tags.begin(), sel0->tags.end(), trimmed)
                                  != sel0->tags.end();
                    if (!exists) {
                        ctx.pushUndo();
                        sel0->tags.push_back(trimmed);
                        ctx.markModified();
                    }
                }
                newTagBuf[0] = '\0';
            }
        }

        // States (C10: per-object state system)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("States");
            ImGui::SameLine();
            ImGui::TextDisabled("(%d)", static_cast<int>(sel0->states.size()));

            auto& states = sel0->states;
            std::string toDelete;

            for (auto& [stId, st] : states) {
                ImGui::PushID(stId.c_str());

                bool open = ImGui::CollapsingHeader(stId.c_str(), ImGuiTreeNodeFlags_None);

                // Inline action buttons (always visible, right side)
                float bw = ImGui::GetContentRegionAvail().x;
                ImGui::SameLine(bw - 90.0f);
                if (ImGui::SmallButton("Apply##stap")) {
                    ctx.pushUndo();
                    if (st.position) sel0->transform.position = *st.position;
                    if (st.rotation) sel0->transform.rotation = *st.rotation;
                    if (st.scale)    sel0->transform.scale    = *st.scale;
                    if (st.visible.has_value()) sel0->visible = *st.visible;
                    if (st.material) sel0->material = *st.material;
                    ctx.markModified();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply this state to the object now");
                ImGui::SameLine();
                if (ImGui::SmallButton("Cap##stcp")) {
                    ctx.pushUndo();
                    if (st.position) st.position = sel0->transform.position;
                    if (st.rotation) st.rotation = sel0->transform.rotation;
                    if (st.scale)    st.scale    = sel0->transform.scale;
                    if (st.visible.has_value()) st.visible = sel0->visible;
                    if (st.material) st.material = sel0->material;
                    ctx.markModified();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Re-capture from current object values");
                ImGui::SameLine();
                if (ImGui::SmallButton("Del##stdel")) {
                    ctx.pushUndo(); toDelete = stId; ctx.markModified();
                }

                if (open) {
                    ImGui::Indent();

                    // ── Position ────────────────────────────────────────────
                    {
                        bool hasP = st.position.has_value();
                        if (ImGui::Checkbox("Position##stp", &hasP)) {
                            ctx.pushUndo();
                            if (hasP) st.position = sel0->transform.position;
                            else      st.position.reset();
                            ctx.markModified();
                        }
                        if (st.position) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1);
                            // SYS-W14-16: was bound straight to st.position's
                            // live storage with pushUndo() nested inside the
                            // changed-check and no IsItemActivated() gate --
                            // found by a dedicated undo-coverage audit. Since
                            // the widget mutates in place every frame of the
                            // drag, that pattern pushed a snapshot ALREADY
                            // containing the new value on every frame, so
                            // Ctrl+Z could never reach the pre-drag value.
                            bool changed = ImGui::DragFloat3("##stpv", st.position->data(), 0.01f);
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            if (changed) ctx.markModified();
                        }
                    }

                    // ── Rotation ────────────────────────────────────────────
                    {
                        bool hasR = st.rotation.has_value();
                        if (ImGui::Checkbox("Rotation##str", &hasR)) {
                            ctx.pushUndo();
                            if (hasR) st.rotation = sel0->transform.rotation;
                            else      st.rotation.reset();
                            ctx.markModified();
                        }
                        if (st.rotation) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1);
                            // SYS-W14-16: same fix as the Position field above.
                            bool changed = ImGui::DragFloat3("##strv", st.rotation->data(), 0.5f);
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            if (changed) ctx.markModified();
                        }
                    }

                    // ── Scale ───────────────────────────────────────────────
                    {
                        bool hasS = st.scale.has_value();
                        if (ImGui::Checkbox("Scale##sts", &hasS)) {
                            ctx.pushUndo();
                            if (hasS) st.scale = sel0->transform.scale;
                            else      st.scale.reset();
                            ctx.markModified();
                        }
                        if (st.scale) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1);
                            // SYS-W14-16: same fix as the Position field above.
                            bool changed = ImGui::DragFloat3("##stsv", st.scale->data(), 0.01f, 0.001f, 1000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            if (changed) ctx.markModified();
                        }
                    }

                    // ── Visible ─────────────────────────────────────────────
                    {
                        bool hasVis = st.visible.has_value();
                        if (ImGui::Checkbox("Visible##stv", &hasVis)) {
                            ctx.pushUndo();
                            if (hasVis) st.visible = sel0->visible;
                            else        st.visible.reset();
                            ctx.markModified();
                        }
                        if (st.visible.has_value()) {
                            ImGui::SameLine();
                            bool v = *st.visible;
                            if (ImGui::Checkbox("##stvv", &v)) {
                                ctx.pushUndo(); st.visible = v; ctx.markModified();
                            }
                        }
                    }

                    // ── Material ────────────────────────────────────────────
                    {
                        bool hasMat = st.material.has_value();
                        if (ImGui::Checkbox("Material##stm", &hasMat)) {
                            ctx.pushUndo();
                            if (hasMat) st.material = sel0->material;
                            else        st.material.reset();
                            ctx.markModified();
                        }
                        if (st.material) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1);
                            // Combo from document materials
                            const std::string& cur = *st.material;
                            if (ImGui::BeginCombo("##stmv", cur.empty() ? "(none)" : cur.c_str())) {
                                if (ImGui::Selectable("(none)", cur.empty())) {
                                    ctx.pushUndo(); st.material = std::string{}; ctx.markModified();
                                }
                                for (const auto& [mk, _] : ctx.document.materials) {
                                    bool sel = (mk == cur);
                                    if (ImGui::Selectable(mk.c_str(), sel)) {
                                        ctx.pushUndo(); *st.material = mk; ctx.markModified();
                                    }
                                    if (sel) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                        }
                    }

                    ImGui::Unindent();
                    ImGui::Spacing();
                }

                ImGui::PopID();
            }
            if (!toDelete.empty()) states.erase(toDelete);

            // Add new state
            ImGui::Separator();
            static char newStateName[64] = {};
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 132);
            ImGui::InputTextWithHint("##stname", "state name…", newStateName, sizeof(newStateName));
            ImGui::SameLine();
            bool nameOk = newStateName[0] != '\0' && !states.count(newStateName);
            if (!nameOk) ImGui::BeginDisabled();
            if (ImGui::SmallButton("+ Empty")) {
                ctx.pushUndo();
                states[newStateName] = Mc3::Mc3ObjectState{};
                ctx.markModified();
                newStateName[0] = '\0';
            }
            if (!nameOk) ImGui::EndDisabled();
            ImGui::SameLine();
            if (!nameOk) ImGui::BeginDisabled();
            if (ImGui::SmallButton("+ Capture")) {
                ctx.pushUndo();
                Mc3::Mc3ObjectState st;
                st.position = sel0->transform.position;
                st.rotation = sel0->transform.rotation;
                st.scale    = sel0->transform.scale;
                st.visible  = sel0->visible;
                if (!sel0->material.empty()) st.material = sel0->material;
                states[newStateName] = st;
                ctx.markModified();
                newStateName[0] = '\0';
            }
            if (!nameOk) ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Capture current transform + visibility + material as a new state");
            if (states.count(newStateName))
                ImGui::TextColored(ImVec4(1.f, 0.5f, 0.2f, 1.f), "Name already exists.");
        }

        ImGui::EndTabItem();
        } // end Transform tab

        if (ImGui::BeginTabItem("Geometry")) {

        // CSG operation
        if (sel0->type == Mc3::ObjectType::Union       ||
            sel0->type == Mc3::ObjectType::Difference  ||
            sel0->type == Mc3::ObjectType::Intersection)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("CSG Operation");

            if (!sel0->csgOperation) {
                Mc3::Mc3CsgOperation csg;
                csg.csgType = (sel0->type == Mc3::ObjectType::Union)      ? Mc3::CsgType::Union
                            : (sel0->type == Mc3::ObjectType::Difference) ? Mc3::CsgType::Difference
                            : Mc3::CsgType::Intersection;
                sel0->csgOperation = csg;
            }
            auto& csg = *sel0->csgOperation;
            const char* csgNames[] = { "Union", "Difference", "Intersection" };
            int csgIdx = static_cast<int>(csg.csgType);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##csgtype", &csgIdx, csgNames, 3)) {
                ctx.pushUndo();
                csg.csgType = static_cast<Mc3::CsgType>(csgIdx);
                sel0->type  = (csgIdx == 0) ? Mc3::ObjectType::Union
                            : (csgIdx == 1) ? Mc3::ObjectType::Difference
                            : Mc3::ObjectType::Intersection;
                ctx.markModified();
            }

            // Children list with drag-reorder (all CSG types; K2)
            if (!sel0->children.empty()) {
                ImGui::Spacing();
                const bool isDiff = (sel0->type == Mc3::ObjectType::Difference);
                ImGui::TextDisabled(isDiff ? "Children (drag=reorder, check=cutter)"
                                           : "Children (drag to reorder)");

                int dragFrom = -1, dragTo = -1;
                for (int ci = 0; ci < (int)sel0->children.size(); ++ci) {
                    auto& child = sel0->children[ci];
                    ImGui::PushID(ci);
                    const std::string& cname = child->name.empty() ? child->id : child->name;

                    if (isDiff) {
                        bool isCut = child->isCutter;
                        if (ImGui::Checkbox("##cut", &isCut)) {
                            ctx.pushUndo();
                            child->isCutter = isCut;
                            ctx.markModified();
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Cutter volume (subtracted from base)");
                        ImGui::SameLine();
                    }

                    ImGui::Selectable((":: " + cname).c_str(), false);

                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        ImGui::SetDragDropPayload("CSG_CHILD_IDX", &ci, sizeof(int));
                        ImGui::Text("Move: %s", cname.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* p =
                                ImGui::AcceptDragDropPayload("CSG_CHILD_IDX")) {
                            dragFrom = *(const int*)p->Data;
                            dragTo   = ci;
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::PopID();
                }

                if (dragFrom >= 0 && dragTo >= 0 && dragFrom != dragTo) {
                    ctx.pushUndo();
                    auto moved = std::move(sel0->children[dragFrom]);
                    sel0->children.erase(sel0->children.begin() + dragFrom);
                    sel0->children.insert(sel0->children.begin() + dragTo, std::move(moved));
                    ctx.markModified();
                }

                // K4: Show tri count of last rendered CSG result
                {
                    int tc = ctx.renderer->csgCachedTriCount(sel0->id);
                    ImGui::Spacing();
                    if (tc >= 0)
                        ImGui::TextDisabled("Tris: %d", tc);
                    else
                        ImGui::TextDisabled("Tris: — (not yet rendered)");
                }

                // STAB-0672: the CSG preview silently drops content it can't
                // represent (Mesh/Extrude children, or nodes past the depth
                // limit) -- "Tris: 0" alone doesn't distinguish that from a
                // legitimately empty boolean, so surface the reason explicitly.
                {
                    std::string warn = ctx.renderer->csgWarning(sel0->id);
                    if (!warn.empty()) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                        ImGui::TextWrapped("⚠ Preview incomplete: %s", warn.c_str());
                        ImGui::PopStyleColor();
                    }
                }

                // K3: Export CSG result as OBJ
                ImGui::Spacing();
                if (ImGui::Button("Export OBJ…")) {
                    ctx.openCsgExport(sel0.get());
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Build CSG result and export as Wavefront OBJ");
            }
        }

        // STAB-0721: Area objects have no dedicated properties block and used
        // to fall through to the shared primitive editor with no indication
        // they're a trigger-zone marker rather than a Box. Areas loaded from
        // XML with a size attribute get primitiveType defaulted to Box
        // (Mc3XmlParser.cpp has no ObjectType::Area case in parsePrimitive's
        // switch), so a plain label here -- not a new case in the switch
        // below -- is the correct minimal fix.
        if (sel0->type == Mc3::ObjectType::Area) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Area (trigger zone)");
        }

        // Geometry parameters
        if (sel0->primitive) {
            ImGui::Spacing();
            if (sel0->type != Mc3::ObjectType::Area) ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Geometry");

            auto& p = *sel0->primitive;

            switch (p.primitiveType) {
            case Mc3::PrimitiveType::Box:
            case Mc3::PrimitiveType::Cube: {
                float sz[3] = { p.size[0], p.size[1], p.size[2] };
                ImGui::TextDisabled("Size (W/H/D)");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh818 = ImGui::DragFloat3("##psize", sz, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh818) {
                    p.size[0] = std::max(0.001f, sz[0]);
                    p.size[1] = std::max(0.001f, sz[1]);
                    p.size[2] = std::max(0.001f, sz[2]);
                    ctx.markModified();
                } }
                break;
            }
            case Mc3::PrimitiveType::Sphere: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh831 = ImGui::DragFloat("##prad", &r, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh831) {
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                { bool _undoChSegs = ImGui::SliderInt("##psegs", &segs, 4, 64, "%d", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoChSegs) {
                    p.segments = segs;
                    ctx.markModified();
                } }
                break;
            }
            case Mc3::PrimitiveType::Cylinder:
            case Mc3::PrimitiveType::Cone: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh851 = ImGui::DragFloat("##prad", &r, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh851) {
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Height");
                float h = p.height;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh859 = ImGui::DragFloat("##phgt", &h, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh859) {
                    p.height = std::max(0.001f, h);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                { bool _undoChSegs = ImGui::SliderInt("##psegs", &segs, 4, 64, "%d", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoChSegs) {
                    p.segments = segs;
                    ctx.markModified();
                } }
                // STAB-0712: Mc3Primitive::axis had no UI anywhere -- Cone
                // doesn't consume it (buildCone() is always Y-axis-only,
                // confirmed by reading MeshBuilder.cpp's buildPrimitive()
                // switch), only Cylinder does, so this is gated by type.
                if (p.primitiveType == Mc3::PrimitiveType::Cylinder)
                    drawAxisCombo(ctx, p, "##pcyl_axis");
                break;
            }
            case Mc3::PrimitiveType::Plane: {
                ImGui::TextDisabled("Width");
                float w = p.size[0];
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh884 = ImGui::DragFloat("##ppw", &w, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh884) {
                    p.size[0] = std::max(0.001f, w);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Depth");
                float d = p.size[2];
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh892 = ImGui::DragFloat("##ppd", &d, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh892) {
                    p.size[2] = std::max(0.001f, d);
                    ctx.markModified();
                } }
                drawAxisCombo(ctx, p, "##ppln_axis");
                break;
            }
            case Mc3::PrimitiveType::Disk: {
                ImGui::TextDisabled("Outer Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh904 = ImGui::DragFloat("##pdsk_r", &r, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh904) {
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Inner Radius (0 = solid)");
                float ir = p.minorRadius;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh912 = ImGui::DragFloat("##pdsk_ir", &ir, 0.01f, 0.0f, p.radius - 0.001f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh912) {
                    p.minorRadius = std::clamp(ir, 0.0f, p.radius - 0.001f);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                { bool _undoChDskSegs = ImGui::SliderInt("##pdsk_segs", &segs, 3, 128, "%d", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoChDskSegs) {
                    p.segments = segs;
                    ctx.markModified();
                } }
                // STAB-0712: buildDisk() also takes an axis param
                // (MeshBuilder.cpp's buildPrimitive() switch) -- the
                // original finding only named Cylinder/Capsule/Plane, Disk
                // is a 4th previously-unlisted instance of the same gap,
                // confirmed directly against the switch before fixing.
                drawAxisCombo(ctx, p, "##pdsk_axis");
                break;
            }
            case Mc3::PrimitiveType::Capsule: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh937 = ImGui::DragFloat("##pcap_r", &r, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh937) {
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Height (cylinder part)");
                float h = p.height;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh945 = ImGui::DragFloat("##pcap_h", &h, 0.01f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh945) {
                    p.height = std::max(0.0f, h);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                { bool _undoChCapSegs = ImGui::SliderInt("##pcap_segs", &segs, 6, 64, "%d", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoChCapSegs) {
                    p.segments = segs;
                    ctx.markModified();
                } }
                drawAxisCombo(ctx, p, "##pcap_axis");
                break;
            }
            case Mc3::PrimitiveType::Grid: {
                ImGui::TextDisabled("Width (X)");
                float sx = p.size[0];
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh965 = ImGui::DragFloat("##pgrd_sx", &sx, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh965) {
                    p.size[0] = std::max(0.001f, sx);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Depth (Z)");
                float sz = p.size[2];
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh973 = ImGui::DragFloat("##pgrd_sz", &sz, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh973) {
                    p.size[2] = std::max(0.001f, sz);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Subdivisions X");
                int subX = p.subdivisionsX;
                ImGui::SetNextItemWidth(-1);
                { bool _undoChSubX = ImGui::SliderInt("##pgrd_subx", &subX, 1, 64, "%d", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoChSubX) {
                    p.subdivisionsX = std::max(1, subX);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Subdivisions Z");
                int subZ = p.subdivisionsZ;
                ImGui::SetNextItemWidth(-1);
                { bool _undoChSubZ = ImGui::SliderInt("##pgrd_subz", &subZ, 1, 64, "%d", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoChSubZ) {
                    p.subdivisionsZ = std::max(1, subZ);
                    ctx.markModified();
                } }
                break;
            }
            case Mc3::PrimitiveType::IcoSphere: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh1000 = ImGui::DragFloat("##pico_r", &r, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh1000) {
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                } }
                // STAB-0711: was a hardcoded, non-interactive "320 triangles
                // (2 subdivisions)" label -- also just plain wrong for this
                // primitive's own real default (segments=32 -> subdivisions
                // 4 -> 5120 triangles, not 2/320; buildIcoSphere() maps
                // segments to subdivisions via segments/8, clamped to
                // [1,4], per mc3togltf/src/MeshBuilder.cpp). Exposes
                // subdivisions directly (the actually meaningful knob, not
                // its 8x-scaled segments encoding) and computes the real
                // resulting triangle count live: 20 * 4^subdivisions.
                int subdivisions = std::clamp(p.segments / 8, 1, 4);
                ImGui::TextDisabled("Subdivisions");
                ImGui::SetNextItemWidth(-1);
                { bool _undoChIcoSub = ImGui::SliderInt("##pico_sub", &subdivisions, 1, 4, "%d", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoChIcoSub) {
                    p.segments = subdivisions * 8;
                    ctx.markModified();
                } }
                int triCount = 20;
                for (int i = 0; i < subdivisions; ++i) triCount *= 4;
                ImGui::TextDisabled("%d triangles", triCount);
                break;
            }
            case Mc3::PrimitiveType::Torus: {
                ImGui::TextDisabled("Major Radius");
                float mr = p.majorRadius;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh1031 = ImGui::DragFloat("##ptor_mr", &mr, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh1031) {
                    p.majorRadius = std::max(0.001f, mr);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Minor Radius");
                float rr = p.minorRadius;
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh1039 = ImGui::DragFloat("##ptor_rr", &rr, 0.01f, 0.001f, p.majorRadius, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh1039) {
                    p.minorRadius = std::clamp(rr, 0.001f, p.majorRadius);
                    ctx.markModified();
                } }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                { bool _undoChTorSegs = ImGui::SliderInt("##ptor_segs", &segs, 4, 64, "%d", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoChTorSegs) {
                    p.segments = segs;
                    ctx.markModified();
                } }
                break;
            }
            }
        }

        // Extrude editor
        if (sel0->type == Mc3::ObjectType::Extrude) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Extrude");

            if (!sel0->extrude) sel0->extrude = Mc3::Mc3Extrude{};
            auto& ex = *sel0->extrude;

            // General params
            ImGui::TextDisabled("Twist (deg)");
            ImGui::SetNextItemWidth(-1);
            { bool _undoCh1070 = ImGui::DragFloat("##extwist", &ex.twist, 1.0f, -3600.0f, 3600.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemActivated()) ctx.pushUndo();
            if (_undoCh1070) {
                ctx.markModified();
            } }
            ImGui::TextDisabled("Path Segments");
            ImGui::SetNextItemWidth(-1);
            { bool _undoChExSegs = ImGui::SliderInt("##exsegs", &ex.segments, 1, 128, "%d", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemActivated()) ctx.pushUndo();
            if (_undoChExSegs) {
                ctx.markModified();
            } }
            // STAB-0719: these two were the only Extrude fields in this
            // block missing pushUndo() -- every sibling field (e.g. the
            // Path Segments slider directly above) already has it.
            //
            // 2026-07-20 audit finding #3: STAB-0719's own fix was still
            // wrong in a subtler way -- ImGui::Checkbox writes *v in place
            // and returns true on the SAME call, so ctx.pushUndo() here ran
            // AFTER ex.smooth/ex.caps already held the new value, making
            // Ctrl+Z a silent no-op (same bug class as
            // MeshCraftApplication_Anim.cpp's act.loop/autoplay had, fixed
            // via the same local-copy-then-writeback shape).
            {
                bool smoothLocal = ex.smooth;
                if (ImGui::Checkbox("Smooth", &smoothLocal)) {
                    ctx.pushUndo(); ex.smooth = smoothLocal; ctx.markModified();
                }
            }
            ImGui::SameLine();
            {
                bool capsLocal = ex.caps;
                if (ImGui::Checkbox("Caps", &capsLocal)) {
                    ctx.pushUndo(); ex.caps = capsLocal; ctx.markModified();
                }
            }

            // --- Cross-section ---
            if (ImGui::TreeNodeEx("Cross-section", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& cs = ex.crossSection;
                const char* csTypes[] = { "Rect", "Circle", "Polygon", "Custom", "Star" };
                int csIdx = static_cast<int>(cs.type);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##cstype", &csIdx, csTypes, 5)) {
                    ctx.pushUndo();
                    cs.type = static_cast<Mc3::CrossSectionType>(csIdx);
                    ctx.markModified();
                }
                switch (cs.type) {
                case Mc3::CrossSectionType::Rect:
                    ImGui::TextDisabled("Width");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1102 = ImGui::DragFloat("##csw", &cs.width, 0.01f, 0.001f, 1000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1102) {
                        cs.width = std::max(0.001f, cs.width);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Height");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1109 = ImGui::DragFloat("##csh", &cs.height, 0.01f, 0.001f, 1000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1109) {
                        cs.height = std::max(0.001f, cs.height);
                        ctx.markModified();
                    } }
                    break;
                case Mc3::CrossSectionType::Circle:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1118 = ImGui::DragFloat("##csr", &cs.radius, 0.01f, 0.001f, 1000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1118) {
                        cs.radius = std::max(0.001f, cs.radius);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Inner Radius");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1125 = ImGui::DragFloat("##csir", &cs.innerRadius, 0.01f, 0.0f, cs.radius, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1125) {
                        cs.innerRadius = std::max(0.0f, cs.innerRadius);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Segments");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoChCsSeg = ImGui::SliderInt("##csseg", &cs.segments, 3, 64, "%d", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoChCsSeg) {
                        ctx.markModified();
                    } }
                    break;
                case Mc3::CrossSectionType::Polygon:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1140 = ImGui::DragFloat("##cspr", &cs.radius, 0.01f, 0.001f, 1000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1140) {
                        cs.radius = std::max(0.001f, cs.radius);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Inner Radius");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1147 = ImGui::DragFloat("##cspir", &cs.innerRadius, 0.01f, 0.0f, cs.radius, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1147) {
                        cs.innerRadius = std::max(0.0f, cs.innerRadius);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Sides");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoChCsPsd = ImGui::SliderInt("##cspsd", &cs.sides, 3, 32, "%d", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoChCsPsd) {
                        ctx.markModified();
                    } }
                    break;
                case Mc3::CrossSectionType::Star:
                    ImGui::TextDisabled("Points (tips)");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoChCsStPts = ImGui::SliderInt("##csstpts", &cs.sides, 3, 16, "%d", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoChCsStPts) {
                        cs.sides = std::max(3, cs.sides);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Outer Radius");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1169 = ImGui::DragFloat("##csstr", &cs.radius, 0.01f, 0.001f, 1000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1169) {
                        cs.radius = std::max(0.001f, cs.radius);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Inner Radius (0 = 50%%)");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1176 = ImGui::DragFloat("##csstir", &cs.innerRadius, 0.01f, 0.0f, cs.radius, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1176) {
                        cs.innerRadius = std::clamp(cs.innerRadius, 0.0f, cs.radius);
                        ctx.markModified();
                    } }
                    break;
                case Mc3::CrossSectionType::Custom:
                    ImGui::TextDisabled("Points (X Y)");
                    for (int pi = 0; pi < static_cast<int>(cs.customPoints.size()); ++pi) {
                        ImGui::PushID(pi);
                        float xy[2] = { cs.customPoints[pi].x, cs.customPoints[pi].y };
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20);
                        { bool _undoCh1188 = ImGui::DragFloat2("##cpt", xy, 0.01f);
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        if (_undoCh1188) {
                            cs.customPoints[pi].x = xy[0];
                            cs.customPoints[pi].y = xy[1];
                            ctx.markModified();
                        } }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("x")) {
                            ctx.pushUndo();
                            cs.customPoints.erase(cs.customPoints.begin() + pi);
                            ctx.markModified();
                            ImGui::PopID(); break;
                        }
                        ImGui::PopID();
                    }
                    if (ImGui::SmallButton("+ Point")) {
                        ctx.pushUndo();
                        cs.customPoints.push_back({0.0f, 0.0f});
                        ctx.markModified();
                    }
                    break;
                }
                ImGui::TreePop();
            }

            // --- Path ---
            if (ImGui::TreeNodeEx("Path", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& path = ex.path;
                const char* pathTypes[] = { "Line", "Arc", "Helix", "Polyline", "Bezier" };
                int ptIdx = static_cast<int>(path.type);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##pathtype", &ptIdx, pathTypes, 5)) {
                    ctx.pushUndo();
                    path.type = static_cast<Mc3::ExtrudePathType>(ptIdx);
                    ctx.markModified();
                }
                switch (path.type) {
                case Mc3::ExtrudePathType::Line: {
                    ImGui::TextDisabled("Length");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1228 = ImGui::DragFloat("##plen", &path.length, 0.01f, 0.001f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1228) {
                        path.length = std::max(0.001f, path.length);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Axis");
                    const char* axes[] = { "x", "y", "z" };
                    int axIdx = (path.axis == "x") ? 0 : (path.axis == "z") ? 2 : 1;
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##paxis", &axIdx, axes, 3)) {
                        ctx.pushUndo();
                        path.axis = axes[axIdx];
                        ctx.markModified();
                    }
                    break;
                }
                case Mc3::ExtrudePathType::Arc:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1247 = ImGui::DragFloat("##parr", &path.arcRadius, 0.01f, 0.001f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1247) {
                        path.arcRadius = std::max(0.001f, path.arcRadius);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Angle (deg)");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1254 = ImGui::DragFloat("##para", &path.arcAngle, 1.0f, -360.0f, 360.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1254) {
                        ctx.markModified();
                    } }
                    break;
                case Mc3::ExtrudePathType::Helix:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1262 = ImGui::DragFloat("##phr", &path.helixRadius, 0.01f, 0.001f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1262) {
                        path.helixRadius = std::max(0.001f, path.helixRadius);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Height");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1269 = ImGui::DragFloat("##phh", &path.helixHeight, 0.01f, 0.001f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1269) {
                        path.helixHeight = std::max(0.001f, path.helixHeight);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Turns");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1276 = ImGui::DragFloat("##pht", &path.helixTurns, 0.1f, 0.1f, 1000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh1276) {
                        path.helixTurns = std::max(0.1f, path.helixTurns);
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Pitch (height per turn)");
                    ImGui::SetNextItemWidth(-1);
                    {
                        float pitch = (path.helixTurns > 0.0f)
                            ? path.helixHeight / path.helixTurns : 0.0f;
                        { bool _undoCh1286 = ImGui::DragFloat("##php", &pitch, 0.01f, 0.001f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        if (_undoCh1286) {
                            pitch = std::max(0.001f, pitch);
                            path.helixHeight = pitch * path.helixTurns;
                            ctx.markModified();
                        } }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Pitch = Height / Turns\nChanging pitch updates Height.");
                    }
                    break;
                case Mc3::ExtrudePathType::Polyline:
                case Mc3::ExtrudePathType::Bezier: {
                    bool isBez = path.type == Mc3::ExtrudePathType::Bezier;
                    ImGui::TextDisabled(isBez ? "Points (pos + ctrl)" : "Points");
                    for (int pi = 0; pi < static_cast<int>(path.points.size()); ++pi) {
                        ImGui::PushID(pi);
                        auto& pt = path.points[pi];
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20);
                        { bool _undoCh1304 = ImGui::DragFloat3("##pp", pt.position.data(), 0.1f);
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        if (_undoCh1304) {
                            ctx.markModified();
                        } }
                        if (isBez) {
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20);
                            { bool _undoCh1310 = ImGui::DragFloat3("##pc", pt.controlIn.data(), 0.1f);
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            if (_undoCh1310) {
                                ctx.markModified();
                            } }
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("x")) {
                            ctx.pushUndo();
                            path.points.erase(path.points.begin() + pi);
                            ctx.markModified();
                            ImGui::PopID(); break;
                        }
                        ImGui::PopID();
                    }
                    if (ImGui::SmallButton("+ Point")) {
                        ctx.pushUndo();
                        Mc3::Mc3PathPoint pp;
                        if (!path.points.empty()) pp.position = path.points.back().position;
                        path.points.push_back(pp);
                        ctx.markModified();
                    }
                    break;
                }
                }
                ImGui::TreePop();
            }
        }

        // Mesh source (F2)
        if (sel0->type == Mc3::ObjectType::Mesh) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Mesh Source");
            char srcBuf[512];
            std::strncpy(srcBuf, sel0->meshSource.c_str(), sizeof(srcBuf)-1); srcBuf[511]='\0';
            ImGui::SetNextItemWidth(-46);
            if (ImGui::InputText("##meshsrc", srcBuf, sizeof(srcBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                ctx.pushUndo(); sel0->meshSource = srcBuf; ctx.markModified();
            }
            ImGui::SameLine(0, 4);
            if (ImGui::Button("...##meshbrw", ImVec2(38, 0))) {
                ctx.openMeshBrowse(sel0->meshSource);
            }
            ImGui::SetItemTooltip("Browse for mesh file (.obj / .glb / .gltf)");
        }

        // Instance definition + material override
        if (sel0->type == Mc3::ObjectType::Instance) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Definition");
            const char* defPreview = sel0->definition.empty() ? "(none)" : sel0->definition.c_str();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##instdef", defPreview)) {
                if (ImGui::Selectable("(none)", sel0->definition.empty())) {
                    ctx.pushUndo(); sel0->definition = ""; ctx.markModified();
                }
                for (const auto& [defId, _] : ctx.document.definitions) {
                    bool selected = (defId == sel0->definition);
                    if (ImGui::Selectable(defId.c_str(), selected)) {
                        ctx.pushUndo(); sel0->definition = defId; ctx.markModified();
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // SYS-W14-29: this is a viewport policy, not serialized scene
            // content, so it has no undo entry and does not mark the scene
            // modified. It swaps authored definition tiers; the renderer's
            // older primitive tessellation LOD remains separate.
            if (ctx.renderer && ImGui::CollapsingHeader("Asset Definition LOD")) {
                ImGui::TextDisabled("Authored definition tiers (not primitive tessellation)");
                auto config = ctx.renderer->assetLodConfig();
                bool configChanged = false;
                configChanged |= ImGui::SliderFloat("Mid distance (m)##assetlod",
                                                    &config.midDistanceM, 0.0f, 500.0f, "%.1f");
                configChanged |= ImGui::SliderFloat("Far distance (m)##assetlod",
                                                    &config.farDistanceM, 0.0f, 1000.0f, "%.1f");
                configChanged |= ImGui::SliderFloat("Hysteresis (m)##assetlod",
                                                    &config.hysteresisM, 0.0f, 50.0f, "%.1f");
                if (configChanged) ctx.renderer->setAssetLodConfig(config);

                const auto debug = ctx.renderer->lastAssetLodSelection(sel0->id);
                if (!debug) {
                    ImGui::TextDisabled("Debug: awaiting viewport draw");
                } else {
                    ImGui::Separator();
                    ImGui::TextDisabled("Selected tier: %s",
                                        std::string(assetLodTierNameAlg(debug->tier)).c_str());
                    ImGui::TextDisabled("Resolved definition: %s",
                                        debug->definitionId.empty() ? "(none)" : debug->definitionId.c_str());
                    if (debug->culled)
                        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Culled");
                    ImGui::TextWrapped("%s", debug->reason.c_str());
                }
            }

            // Definition content preview
            if (!sel0->definition.empty()) {
                auto defIt = ctx.document.definitions.find(sel0->definition);
                if (defIt != ctx.document.definitions.end() && defIt->second) {
                    const auto& defRoot = *defIt->second;
                    std::function<int(const Mc3::Mc3Object&)> countNodes;
                    countNodes = [&](const Mc3::Mc3Object& n) -> int {
                        int c = 1;
                        for (const auto& ch : n.children) c += countNodes(*ch);
                        return c;
                    };
                    int nodeCount = countNodes(defRoot);
                    ImGui::Spacing();
                    ImGui::TextDisabled("Definition content (%d object%s)", nodeCount, nodeCount == 1 ? "" : "s");
                    int rows = std::min(nodeCount, 8);
                    ImVec2 listSize(-1, rows * ImGui::GetTextLineHeightWithSpacing() + 6);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.f));
                    if (ImGui::BeginChild("##defpreview", listSize, true)) {
                        std::function<void(const Mc3::Mc3Object&, int)> showTree;
                        showTree = [&](const Mc3::Mc3Object& node, int depth) {
                            std::string indent(depth * 2, ' ');
                            const char* label = node.name.empty() ? node.id.c_str() : node.name.c_str();
                            ImGui::TextDisabled("%s%s", indent.c_str(), label);
                            for (const auto& ch : node.children)
                                showTree(*ch, depth + 1);
                        };
                        showTree(defRoot, 0);
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                }
            }
            // Variants list (random pool)
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Random Variants (%d)", static_cast<int>(sel0->variantDefinitions.size()));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Each instance picks one variant deterministically\nbased on its ID hash. Empty = use Definition above.");
            // Show existing variants with Remove buttons
            for (int vi = 0; vi < static_cast<int>(sel0->variantDefinitions.size()); ++vi) {
                ImGui::PushID(vi);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 26);
                std::string& vref = sel0->variantDefinitions[vi];
                char vbuf[128]; std::strncpy(vbuf, vref.c_str(), sizeof(vbuf)-1); vbuf[127]='\0';
                if (ImGui::InputText("##vdef", vbuf, sizeof(vbuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    ctx.pushUndo(); vref = vbuf; ctx.markModified();
                }
                ImGui::SameLine(0, 4);
                if (ImGui::SmallButton("x")) {
                    ctx.pushUndo();
                    sel0->variantDefinitions.erase(sel0->variantDefinitions.begin() + vi);
                    ctx.markModified();
                    ImGui::PopID(); break;
                }
                ImGui::PopID();
            }
            // Add variant combo
            {
                static char addVarBuf[128]{};
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50);
                ImGui::InputTextWithHint("##addvar", "definition id…", addVarBuf, sizeof(addVarBuf));
                ImGui::SameLine(0, 4);
                if (ImGui::Button("+Add", ImVec2(46, 0)) && addVarBuf[0]) {
                    ctx.pushUndo();
                    sel0->variantDefinitions.emplace_back(addVarBuf);
                    addVarBuf[0] = '\0';
                    ctx.markModified();
                }
                // Quick-add from existing definitions
                if (ImGui::BeginCombo("##vardefpick", nullptr, ImGuiComboFlags_NoPreview)) {
                    for (const auto& [defId, _] : ctx.document.definitions) {
                        if (ImGui::Selectable(defId.c_str())) {
                            ctx.pushUndo();
                            sel0->variantDefinitions.push_back(defId);
                            ctx.markModified();
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pick from existing definitions");
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Material Override");
            char moBuf[128];
            std::strncpy(moBuf, sel0->materialOverride.c_str(), sizeof(moBuf)-1); moBuf[127]='\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##instmo", moBuf, sizeof(moBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                ctx.pushUndo(); sel0->materialOverride = moBuf; ctx.markModified();
            }
        }

        // Deform (geometry-level non-uniform scale)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            bool deformEnabled = sel0->deform.has_value();
            if (ImGui::Checkbox("Deform", &deformEnabled)) {
                ctx.pushUndo();
                if (deformEnabled) sel0->deform = Mc3::Mc3Deform{};
                else               sel0->deform.reset();
                ctx.markModified();
            }
            if (sel0->deform.has_value()) {
                auto& d = *sel0->deform;
                ImGui::TextDisabled("Deform Scale");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh1490 = ImGui::DragFloat3("##deform", d.scale.data(), 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                if (_undoCh1490) {
                    d.scale[0] = std::max(0.001f, d.scale[0]);
                    d.scale[1] = std::max(0.001f, d.scale[1]);
                    d.scale[2] = std::max(0.001f, d.scale[2]);
                    ctx.markModified();
                } }
            }
        }

        // Poly stats (C6)
        {
            int v = 0, t = 0;
            ctx.renderer->objectPolyStats(*sel0, v, t);
            if (v > 0 || t > 0) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Vertices: %d   Triangles: %d", v, t);
            }
        }

        ImGui::EndTabItem();
        } // end Geometry tab

        if (ImGui::BeginTabItem("Material")) {

        // Material editor
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Material");

            // Assignment dropdown + New button
            {
                bool matEdMixed = !allMatchStr([](const Mc3::Mc3Object* o){ return o->material; });
                const char* preview = matEdMixed ? "(mixed)" : (sel0->material.empty() ? "(none)" : sel0->material.c_str());
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 36);
                if (ImGui::BeginCombo("##matsel", preview)) {
                    if (ImGui::Selectable("(none)", !matEdMixed && sel0->material.empty())) {
                        ctx.pushUndo();
                        for (const auto& s : selAll) s->material = "";
                        ctx.markModified();
                    }
                    for (const auto& [key, _] : ctx.document.materials) {
                        bool selected = (!matEdMixed && key == sel0->material);
                        if (ImGui::Selectable(key.c_str(), selected)) {
                            ctx.pushUndo();
                            for (const auto& s : selAll) s->material = key;
                            ctx.markModified();
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("New")) {
                    ctx.pushUndo();
                    // Generate unique key
                    int n = 1;
                    std::string key;
                    do { key = "material_" + std::to_string(n++); }
                    while (ctx.document.materials.count(key));
                    Mc3::Mc3Material newMat;
                    newMat.name = key;
                    ctx.document.materials[key] = newMat;
                    sel0->material = key;
                    ctx.markModified();
                }
            }

            // Edit the assigned material's fields inline
            // STAB-0719: this ENTIRE block (every field below, including
            // the 5 texture-path fields in texField()) was found during a
            // systematic undo-coverage audit to be completely missing
            // pushUndo() -- unlike the standalone "Mat" tab in the left
            // panel (MeshCraftApplication_UiLeftPanel.cpp), which already
            // has it for the equivalent fields, this separate inline
            // editor (shown when an object with an assigned material is
            // selected) had zero undo coverage for any of its ~14 fields.
            // Fixed using this file's own established convention:
            // Checkbox/Combo/InputText get an unconditional pushUndo() (one
            // fire per click/commit); ColorEdit/Slider/DragFloat get an
            // IsItemActivated()-gated one (fires continuously while
            // dragging, so gate to one undo step per drag session) --
            // matches e.g. this same file's Extrude segments slider and
            // the left panel's light-color ColorEdit3.
            //
            // 2026-07-20 audit finding #3: this rule is incomplete as
            // stated for Checkbox specifically. Combo/InputText are safe
            // with a plain `if (Widget(...)) { pushUndo(); ...write-back...; }`
            // because their normal usage shape already requires an
            // intermediate variable (an index into a local for Combo, a
            // char buffer for InputText) that's explicitly written back to
            // the live field AFTER pushUndo() -- but ImGui::Checkbox(&v)
            // writes *v in place and returns true on the SAME call, so
            // binding it directly to a live field's address (compiles
            // fine, looks identical to the "safe" shape above) mutates the
            // field BEFORE pushUndo() ever runs, making the undo snapshot
            // capture the NEW value. Checkbox must always be bound to a
            // fresh local bool with the write-back inside the `if`, same
            // as this block's own "Double Sided" checkbox below.
            auto matIt = ctx.document.materials.find(sel0->material);
            if (matIt != ctx.document.materials.end()) {
                auto& mat = matIt->second;

                // F8: an edit to a material merged in from an <include> must
                // survive Save -- the writer skips any id still present in
                // doc.includedMaterials, so editing it in place here without
                // promoting it to local silently drops the edit on the next
                // save (same fix as MeshCraftApplication_UiLeftPanel.cpp's
                // "Mat" tab, applied to this SEPARATE inline material
                // editor). matIt->first is the material's actual key
                // (== sel0->material, already resolved by the find() above).
                auto pushUndoMat = [&]() {
                    ctx.pushUndo();
                    ctx.document.includedMaterials.erase(matIt->first);
                };
                if (ctx.document.includedMaterials.count(matIt->first))
                    ImGui::TextDisabled("(from <include> -- editing makes a local copy)");

                // Base color
                ImGui::TextDisabled("Base Color");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh1623 = ImGui::ColorEdit4("##mbc", mat.baseColor.data(),
                        ImGuiColorEditFlags_NoLabel);
                    if (ImGui::IsItemActivated()) pushUndoMat();
                    if (_undoCh1623) {
                    ctx.markModified();
                } }

                // Roughness
                ImGui::TextDisabled("Roughness");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: without it, Ctrl+Click lets a typed value go
                // out of [0,1], which would export a spec-invalid glTF
                // pbrMetallicRoughness.roughnessFactor with no other guard.
                { bool _undoChRough = ImGui::SliderFloat("##mrough", &mat.roughness, 0.0f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) pushUndoMat();
                if (_undoChRough) {
                    ctx.markModified();
                } }

                // Metallic
                ImGui::TextDisabled("Metallic");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: same out-of-[0,1]-via-Ctrl+Click risk as
                // roughness above (spec-invalid metallicFactor on export).
                { bool _undoChMetal = ImGui::SliderFloat("##mmetal", &mat.metallic, 0.0f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) pushUndoMat();
                if (_undoChMetal) {
                    ctx.markModified();
                } }

                // Emissive color
                ImGui::TextDisabled("Emissive");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh1655 = ImGui::ColorEdit3("##memit", mat.emissiveColor.data(),
                        ImGuiColorEditFlags_NoLabel);
                    if (ImGui::IsItemActivated()) pushUndoMat();
                    if (_undoCh1655) {
                    ctx.markModified();
                } }

                // Alpha mode
                ImGui::TextDisabled("Alpha Mode");
                ImGui::SetNextItemWidth(-1);
                const char* alphaModes[] = { "opaque", "mask", "blend" };
                int alphaIdx = 0;
                for (int i = 0; i < 3; ++i)
                    if (mat.alphaMode == alphaModes[i]) { alphaIdx = i; break; }
                if (ImGui::Combo("##malpha", &alphaIdx, alphaModes, 3)) {
                    pushUndoMat();
                    mat.alphaMode = alphaModes[alphaIdx];
                    ctx.markModified();
                }
                if (mat.alphaMode == "mask") {
                    ImGui::TextDisabled("Alpha Cutoff");
                    ImGui::SetNextItemWidth(-1);
                    // AlwaysClamp: same out-of-[0,1]-via-Ctrl+Click risk as
                    // roughness/metallic above (spec-invalid alphaCutoff).
                    { bool _undoChCutoff = ImGui::SliderFloat("##mcut", &mat.alphaCutoff, 0.0f, 1.0f, "%.3f",
                                           ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) pushUndoMat();
                    if (_undoChCutoff) {
                        ctx.markModified();
                    } }
                }

                // Double sided
                // 2026-07-20 audit finding #3: local-copy-then-writeback --
                // Checkbox writes in place and returns true on the SAME
                // call, so pushUndoMat() used to run after the value was
                // already changed (silent no-op Ctrl+Z). Same established
                // fix as MeshCraftApplication_Anim.cpp's act.loop/autoplay.
                {
                    bool doubleSidedLocal = mat.doubleSided;
                    if (ImGui::Checkbox("Double Sided", &doubleSidedLocal)) {
                        pushUndoMat();
                        mat.doubleSided = doubleSidedLocal;
                        ctx.markModified();
                    }
                }

                // Normal scale + occlusion strength (collapsed by default)
                if (ImGui::TreeNode("Advanced")) {
                    ImGui::TextDisabled("Normal Scale");
                    ImGui::SetNextItemWidth(-1);
                    // AlwaysClamp: same out-of-range-via-Ctrl+Click risk as
                    // roughness/metallic/alphaCutoff above -- occlusionStrength
                    // in particular is glTF-spec-bounded to [0,1].
                    { bool _undoCh1659 = ImGui::DragFloat("##mnrmscl", &mat.normalScale, 0.01f, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) pushUndoMat();
                    if (_undoCh1659) {
                        ctx.markModified();
                    } }
                    ImGui::TextDisabled("Occlusion Strength");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh1665 = ImGui::DragFloat("##moccstr", &mat.occlusionStrength, 0.01f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) pushUndoMat();
                    if (_undoCh1665) {
                        ctx.markModified();
                    } }
                    ImGui::TreePop();
                }

                // Textures (collapsed by default)
                if (ImGui::TreeNode("Textures")) {
                    bool hasPendingTex = !ctx.pendingDropTexture.empty();
                    // texField: tracks hover for OS drag-drop (D6)
                    // SYS-W14-15: native file-browse "..." button, one line
                    // this file previously called out as missing entirely
                    // (every texture path was drag-drop or manual-typed
                    // only). Gated on FileDialog's own platform-support
                    // check (false on Web/iOS, where no native backend
                    // exists) so those builds keep the manual-entry-only
                    // field instead of a dead button.
                    const bool canBrowse = CNA::Devices::FileDialog::getIsSupportedProperty();
                    auto texField = [&](const char* label, std::string& field, const char* slot) {
                        ImGui::TextDisabled("%s", label);
                        char buf[512];
                        std::strncpy(buf, field.c_str(), sizeof(buf) - 1); buf[511] = '\0';
                        ImGui::SetNextItemWidth(canBrowse ? -32.0f : -1.0f);
                        // Highlight field that is the current drop target
                        bool isHov = (ctx.hoveredTexSlot == slot && ctx.hoveredTexMatId == ctx.selectedMaterialKey);
                        if (isHov && hasPendingTex)
                            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.5f, 0.2f, 0.6f));
                        std::string id = std::string("##t") + label;
                        if (ImGui::InputText(id.c_str(), buf, sizeof(buf),
                                ImGuiInputTextFlags_EnterReturnsTrue)) {
                            pushUndoMat();
                            field = buf; ctx.markModified();
                        }
                        if (isHov && hasPendingTex) ImGui::PopStyleColor();
                        if (ImGui::IsItemHovered()) {
                            ctx.hoveredTexSlot  = slot;
                            ctx.hoveredTexMatId = ctx.selectedMaterialKey;
                        }
                        if (canBrowse) {
                            ImGui::SameLine();
                            std::string btnId = std::string("...##b") + label;
                            if (ImGui::Button(btnId.c_str()))
                                ctx.browseForTexture(ctx.selectedMaterialKey, slot);
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Browse for a file (native OS dialog)");
                        }
                    };
                    if (hasPendingTex)
                        ImGui::TextColored(ImVec4(0.3f,0.9f,0.4f,1.f),
                            "Drop: hover a slot then release");
                    texField("Base Color",      mat.baseColorTexture,         "base");
                    texField("Normal",          mat.normalTexture,            "normal");
                    texField("Emissive",        mat.emissiveTexture,          "emissive");
                    texField("Metal/Roughness", mat.metallicRoughnessTexture, "metalrough");
                    texField("Occlusion",       mat.occlusionTexture,         "occlusion");
                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndTabItem();
        } // end Material tab

        if (ImGui::BeginTabItem("Anim")) {
        if (!ctx.showTimeline || ctx.currentActionName.empty()) {
            ImGui::TextDisabled("No action selected.");
            ImGui::TextDisabled("Open Timeline and select an animation action.");
        } else {
            using AP = Mc3::AnimatedProperty;

            // Helper: check if this object has a channel for the given property in the current action
            const auto& act = ctx.document.actions.at(ctx.currentActionName);
            auto hasChan = [&](AP prop) -> bool {
                for (const auto& ch : act.channels)
                    if (ch.targetObject == sel0->name && ch.property == prop) return true;
                return false;
            };
            auto hasAnyOf = [&](std::initializer_list<AP> props) -> bool {
                for (auto p : props) if (hasChan(p)) return true;
                return false;
            };

            // Dot indicator colour: green = channel exists, grey = not yet
            auto dot = [&](std::initializer_list<AP> props) {
                if (hasAnyOf(props))
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.f), "\xe2\x97\x8f");
                else
                    ImGui::TextDisabled("\xe2\x97\x8f");
                ImGui::SameLine();
            };

            // ── K All ──────────────────────────────────────────────────────
            if (ImGui::SmallButton("K All")) {
                ctx.insertKeyframes(*sel0, {
                    AP::PositionX, AP::PositionY, AP::PositionZ,
                    AP::RotationX, AP::RotationY, AP::RotationZ,
                    AP::ScaleX,    AP::ScaleY,    AP::ScaleZ,
                    AP::Visible,
                    AP::DeformX,   AP::DeformY,   AP::DeformZ,
                    AP::MaterialBaseColorR, AP::MaterialBaseColorG,
                    AP::MaterialBaseColorB, AP::MaterialBaseColorA,
                    AP::MaterialRoughness,  AP::MaterialMetallic,
                    AP::MaterialEmissiveR,  AP::MaterialEmissiveG,  AP::MaterialEmissiveB,
                });
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Insert keyframe for every animatable property");

            ImGui::Spacing();
            ImGui::Separator();

            // ── Transform ──────────────────────────────────────────────────
            ImGui::TextDisabled("Transform");

            // Position group + per-axis
            dot({AP::PositionX, AP::PositionY, AP::PositionZ});
            if (ImGui::SmallButton("K Pos"))
                ctx.insertKeyframes(*sel0, {AP::PositionX, AP::PositionY, AP::PositionZ});
            ImGui::SameLine();
            { bool a = hasChan(AP::PositionX);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KX##px")) ctx.insertKeyframes(*sel0, {AP::PositionX});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::PositionY);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KY##py")) ctx.insertKeyframes(*sel0, {AP::PositionY});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::PositionZ);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KZ##pz")) ctx.insertKeyframes(*sel0, {AP::PositionZ});
              if (a) ImGui::PopStyleColor(); }

            // Rotation group + per-axis
            dot({AP::RotationX, AP::RotationY, AP::RotationZ});
            if (ImGui::SmallButton("K Rot"))
                ctx.insertKeyframes(*sel0, {AP::RotationX, AP::RotationY, AP::RotationZ});
            ImGui::SameLine();
            { bool a = hasChan(AP::RotationX);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KX##rx")) ctx.insertKeyframes(*sel0, {AP::RotationX});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::RotationY);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KY##ry")) ctx.insertKeyframes(*sel0, {AP::RotationY});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::RotationZ);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KZ##rz")) ctx.insertKeyframes(*sel0, {AP::RotationZ});
              if (a) ImGui::PopStyleColor(); }

            // Scale group + per-axis
            dot({AP::ScaleX, AP::ScaleY, AP::ScaleZ});
            if (ImGui::SmallButton("K Scl"))
                ctx.insertKeyframes(*sel0, {AP::ScaleX, AP::ScaleY, AP::ScaleZ});
            ImGui::SameLine();
            { bool a = hasChan(AP::ScaleX);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KX##sx")) ctx.insertKeyframes(*sel0, {AP::ScaleX});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::ScaleY);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KY##sy")) ctx.insertKeyframes(*sel0, {AP::ScaleY});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::ScaleZ);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KZ##sz")) ctx.insertKeyframes(*sel0, {AP::ScaleZ});
              if (a) ImGui::PopStyleColor(); }

            // Visibility
            dot({AP::Visible});
            if (ImGui::SmallButton("K Vis"))
                ctx.insertKeyframes(*sel0, {AP::Visible});

            ImGui::Spacing();
            ImGui::Separator();

            // ── Deform ─────────────────────────────────────────────────────
            ImGui::TextDisabled("Deform");
            dot({AP::DeformX, AP::DeformY, AP::DeformZ});
            if (ImGui::SmallButton("K Deform"))
                ctx.insertKeyframes(*sel0, {AP::DeformX, AP::DeformY, AP::DeformZ});
            ImGui::SameLine();
            { bool a = hasChan(AP::DeformX);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KX##dx")) ctx.insertKeyframes(*sel0, {AP::DeformX});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::DeformY);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KY##dy")) ctx.insertKeyframes(*sel0, {AP::DeformY});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::DeformZ);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KZ##dz")) ctx.insertKeyframes(*sel0, {AP::DeformZ});
              if (a) ImGui::PopStyleColor(); }

            ImGui::Spacing();
            ImGui::Separator();

            // ── Material ───────────────────────────────────────────────────
            ImGui::TextDisabled("Material");
            dot({AP::MaterialBaseColorR, AP::MaterialBaseColorG,
                 AP::MaterialBaseColorB, AP::MaterialBaseColorA});
            if (ImGui::SmallButton("K Color"))
                ctx.insertKeyframes(*sel0, {AP::MaterialBaseColorR, AP::MaterialBaseColorG,
                                            AP::MaterialBaseColorB, AP::MaterialBaseColorA});
            ImGui::SameLine();
            dot({AP::MaterialEmissiveR, AP::MaterialEmissiveG, AP::MaterialEmissiveB});
            if (ImGui::SmallButton("K Emit"))
                ctx.insertKeyframes(*sel0, {AP::MaterialEmissiveR, AP::MaterialEmissiveG,
                                            AP::MaterialEmissiveB});
            dot({AP::MaterialRoughness});
            if (ImGui::SmallButton("K Rough"))
                ctx.insertKeyframes(*sel0, {AP::MaterialRoughness});
            ImGui::SameLine();
            dot({AP::MaterialMetallic});
            if (ImGui::SmallButton("K Metal"))
                ctx.insertKeyframes(*sel0, {AP::MaterialMetallic});
        }
        ImGui::EndTabItem();
        } // end Anim tab

        if (ImGui::BeginTabItem("UV")) {
        bool hasUV = sel0->uvMapping.has_value();
        if (ImGui::Checkbox("Enable UV mapping##uven", &hasUV)) {
            ctx.pushUndo();
            if (hasUV) sel0->uvMapping = Mc3::Mc3UvMapping{};
            else       sel0->uvMapping.reset();
            ctx.markModified();
        }
        if (sel0->uvMapping) {
            auto& m = *sel0->uvMapping;
            ImGui::Spacing();
            ImGui::Text("Projection:");
            ImGui::SameLine();
            int proj = static_cast<int>(m.projection);
            const char* projNames[] = {"Planar", "Box", "Sphere"};
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::Combo("##uvproj", &proj, projNames, 3)) {
                ctx.pushUndo();
                m.projection = static_cast<Mc3::UvProjection>(proj);
                ctx.markModified();
            }
            ImGui::Spacing();
            ImGui::Text("Scale  U/V:");
            float sc[2] = {m.scaleU, m.scaleV};
            ImGui::SetNextItemWidth(130.0f);
            if (ImGui::DragFloat2("##uvsc", sc, 0.01f, 0.001f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                ctx.pushUndo(); m.scaleU = sc[0]; m.scaleV = sc[1]; ctx.markModified();
            }
            ImGui::Text("Offset U/V:");
            float of[2] = {m.offsetU, m.offsetV};
            ImGui::SetNextItemWidth(130.0f);
            if (ImGui::DragFloat2("##uvof", of, 0.005f, -100.0f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                ctx.pushUndo(); m.offsetU = of[0]; m.offsetV = of[1]; ctx.markModified();
            }
            ImGui::Text("Rotation:  ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            bool uvRotChanged = ImGui::DragFloat("##uvrot", &m.rotation, 0.5f, -360.0f, 360.0f, "%.1f\xc2\xb0", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemActivated()) ctx.pushUndo();
            if (uvRotChanged) ctx.markModified();
        }
        ImGui::EndTabItem();
        } // end UV tab

        ImGui::EndTabBar();
        } // end tab bar

    } else {
        // ------------------------------------------------------------------
        // Scene Properties — shown when nothing is selected
        // ------------------------------------------------------------------
        ImGui::TextColored(ImVec4(0.75f, 0.85f, 1.0f, 1.0f), "Scene Properties");
        ImGui::Separator();
        ImGui::Spacing();

        // Scene name
        {
            char nameBuf[128];
            std::strncpy(nameBuf, ctx.document.model.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf)-1] = '\0';
            ImGui::TextDisabled("Name");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##scname", nameBuf, sizeof(nameBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                ctx.pushUndo();
                ctx.document.model = nameBuf;
                ctx.markModified();
            }
        }

        // Unit
        {
            const char* unitOpts[] = {
                "meter", "centimeter", "millimeter", "inch", "foot"
            };
            int unitIdx = 0;
            for (int i = 0; i < 5; ++i)
                if (ctx.document.unit == unitOpts[i]) { unitIdx = i; break; }
            ImGui::TextDisabled("Unit");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##scunit", &unitIdx, unitOpts, 5)) {
                ctx.pushUndo();
                ctx.document.unit = unitOpts[unitIdx];
                ctx.markModified();
            }
        }

        // Coordinate system. STAB-0713 removed the old invalid
        // left_handed_y_up option: mc3.xsd permits only these two right-handed
        // conventions. Both are now honored by the viewport, interaction and
        // glTF export. Changing the declaration is intentionally semantic; the
        // explicit Normalize action below is available when authored values
        // should be converted to Y-up instead.
        {
            const char* csOpts[] = {
                "right_handed_y_up",
                "right_handed_z_up",
            };
            int csIdx = 0;
            for (int i = 0; i < 2; ++i)
                if (ctx.document.coordinateSystem == csOpts[i]) { csIdx = i; break; }
            ImGui::TextDisabled("Coord System");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##sccs", &csIdx, csOpts, 2)) {
                ctx.pushUndo();
                ctx.document.coordinateSystem = csOpts[csIdx];
                ctx.markModified();
            }
            ImGui::SetItemTooltip("Honored by the viewport, picking, walk collision and glTF export.");
            if (usesRightHandedZUpAlg(ctx.document.coordinateSystem)) {
                if (ImGui::Button("Normalize to Y-up")) {
                    ctx.pushUndo();
                    normalizeCoordinateSystemToYUpAlg(ctx.document);
                    ctx.markModified();
                }
                ImGui::SetItemTooltip(
                    "Convert scene-level cameras/lights and wrap authored objects in an explicit "
                    "-90 degree X group so the visible scene stays unchanged.");
            }
        }

        // Rotation convention. Changing either declaration intentionally
        // changes the interpretation of authored triples; use the explicit
        // Normalize action when a static document should retain its current
        // visual result while moving to degrees/XYZ.
        {
            const char* unitOpts[] = {"degrees", "radians"};
            int unitIdx = ctx.document.rotationUnits == "radians" ? 1 : 0;
            ImGui::TextDisabled("Rotation Units");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##scrotationunits", &unitIdx, unitOpts, 2)) {
                ctx.pushUndo();
                ctx.document.rotationUnits = unitOpts[unitIdx];
                ctx.markModified();
            }
            ImGui::SetItemTooltip(
                "Honored by rendering, picking, gizmos, cameras and animation. "
                "Changing this declaration does not convert existing values.");

            const char* orderOpts[] = {"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"};
            int orderIdx = 0;
            for (int i = 0; i < 6; ++i)
                if (ctx.document.eulerOrder == orderOpts[i]) { orderIdx = i; break; }
            ImGui::TextDisabled("Euler Order");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##sceulerorder", &orderIdx, orderOpts, 6)) {
                ctx.pushUndo();
                ctx.document.eulerOrder = orderOpts[orderIdx];
                ctx.markModified();
            }
            ImGui::SetItemTooltip(
                "The order in which X/Y/Z rotations are applied. "
                "Changing it does not convert existing values.");

            const bool alreadyNormal = ctx.document.rotationUnits == "degrees" &&
                normalisedEulerOrderAlg(ctx.document.eulerOrder) == "XYZ";
            const bool animatedRotation = hasAnimatedRotationAlg(ctx.document);
            const bool canNormalize = !alreadyNormal && !animatedRotation;
            if (!canNormalize) ImGui::BeginDisabled();
            if (ImGui::Button("Normalize rotation to degrees/XYZ")) {
                ctx.pushUndo();
                normalizeRotationConventionToDegreesXYZAlg(ctx.document);
                ctx.markModified();
            }
            if (!canNormalize) ImGui::EndDisabled();
            ImGui::SetItemTooltip(animatedRotation
                ? "Unavailable: Euler rotation animation is preserved unchanged. "
                  "The live editor already honors its document convention."
                : alreadyNormal
                    ? "This document already uses degrees/XYZ."
                    : "Bake static object, state, definition and camera rotations into degrees/XYZ "
                      "without changing their visual result.");
        }

        // Default camera
        if (!ctx.document.cameras.empty()) {
            ImGui::TextDisabled("Default Camera");
            const char* dcPrev = ctx.document.defaultCamera.empty()
                ? "(none)" : ctx.document.defaultCamera.c_str();
            ImGui::SetNextItemWidth(-1);
            // SYS-W14-16: found by a dedicated undo-coverage audit -- every
            // sibling field in this block (Name/Unit/Coord System) calls
            // both pushUndo() and ctx.markModified(); this combo called
            // neither, so picking/clearing the default camera couldn't be
            // undone and didn't even mark the document dirty.
            if (ImGui::BeginCombo("##scdc", dcPrev)) {
                if (ImGui::Selectable("(none)", ctx.document.defaultCamera.empty())) {
                    ctx.pushUndo();
                    ctx.document.defaultCamera = "";
                    ctx.markModified();
                }
                for (const auto& cam : ctx.document.cameras) {
                    bool sel = (cam.name == ctx.document.defaultCamera);
                    if (ImGui::Selectable(cam.name.c_str(), sel)) {
                        ctx.pushUndo();
                        ctx.document.defaultCamera = cam.name;
                        ctx.markModified();
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // SYS-W14-13: R110's doc.library (Mc3LibraryInfo -- namespace,
        // version, contentHash) had zero editor UI. Optional -- most scenes
        // are plain scenes, not reusable .mc3lib libraries -- so gated
        // behind a checkbox that creates/clears ctx.document.library.
        // Importing another library's definitions (doc.imports,
        // Mc3Import) is a separate, ordered-list concern edited in its own
        // "Imports" tab (left panel), not here.
        {
            ImGui::TextColored(ImVec4(0.75f, 0.85f, 1.0f, 1.0f), "Library (.mc3lib)");
            bool hasLib = ctx.document.library.has_value();
            if (ImGui::Checkbox("This document is a reusable library", &hasLib)) {
                ctx.pushUndo();
                if (hasLib) ctx.document.library = Mc3::Mc3LibraryInfo{};
                else        ctx.document.library.reset();
                ctx.markModified();
            }
            if (hasLib) {
                auto& lib = *ctx.document.library;

                ImGui::TextDisabled("Namespace");
                char nsBuf[128];
                std::strncpy(nsBuf, lib.libraryNamespace.c_str(), sizeof(nsBuf)-1); nsBuf[127]='\0';
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##libns", nsBuf, sizeof(nsBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                    ctx.pushUndo(); lib.libraryNamespace = nsBuf; ctx.markModified();
                }

                ImGui::TextDisabled("Version (semver, e.g. 1.0.0)");
                char verBuf[32];
                std::strncpy(verBuf, lib.version.c_str(), sizeof(verBuf)-1); verBuf[31]='\0';
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##libver", verBuf, sizeof(verBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                    ctx.pushUndo(); lib.version = verBuf; ctx.markModified();
                }

                ImGui::TextDisabled("Content Hash");
                ImGui::TextWrapped("%s", lib.contentHash.empty() ? "(not computed)" : lib.contentHash.c_str());
                if (ImGui::SmallButton("Recompute##libhash")) {
                    ctx.pushUndo();
                    lib.contentHash = "sha256:" + ctx.document.computeLibraryContentHash();
                    ctx.markModified();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Hashes this document's content (excluding the library block "
                                       "itself) -- recompute after editing definitions.");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // STAB-0709: N7 meta (doc.meta, arbitrary key/value pairs, mirrors
        // <meta><metaentry key="..." value="..."/> in the XSD) had zero
        // editor UI -- lowest priority of all the N-extensions, a simple
        // key/value list editor. Note doc.metadata (the separate "legacy"
        // pass-through store, XSD's <metadata><property name=... value=.../>)
        // is intentionally out of scope here -- not what this task named.
        {
            ImGui::TextColored(ImVec4(0.75f, 0.85f, 1.0f, 1.0f), "Meta");
            std::vector<std::string> keys;
            keys.reserve(ctx.document.meta.size());
            for (const auto& [k, v] : ctx.document.meta) { (void)v; keys.push_back(k); }

            std::string renameFrom, renameTo;
            std::string removeKey;
            for (const auto& key : keys) {
                auto it = ctx.document.meta.find(key);
                if (it == ctx.document.meta.end()) continue; // already renamed away this frame
                ImGui::PushID(key.c_str());

                char keyBuf[128];
                std::strncpy(keyBuf, key.c_str(), sizeof(keyBuf)-1); keyBuf[127]='\0';
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputText("##metakey", keyBuf, sizeof(keyBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue) && keyBuf[0] != '\0' && key != keyBuf) {
                    renameFrom = key; renameTo = keyBuf;
                }
                ImGui::SameLine();

                char valBuf[256];
                std::strncpy(valBuf, it->second.c_str(), sizeof(valBuf)-1); valBuf[255]='\0';
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##metaval", valBuf, sizeof(valBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                    ctx.pushUndo(); it->second = valBuf; ctx.markModified();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("x##metarm")) removeKey = key;

                ImGui::PopID();
            }
            if (!renameFrom.empty()) {
                ctx.pushUndo();
                auto node = ctx.document.meta.extract(renameFrom);
                node.key() = renameTo;
                ctx.document.meta.insert(std::move(node));
                ctx.markModified();
            }
            if (!removeKey.empty()) {
                ctx.pushUndo();
                ctx.document.meta.erase(removeKey);
                ctx.markModified();
            }
            if (ImGui::SmallButton("+ Add##metaadd")) {
                ctx.pushUndo();
                int n = 1;
                std::string key;
                do { key = "key" + std::to_string(n++); }
                while (ctx.document.meta.count(key));
                ctx.document.meta[key] = "";
                ctx.markModified();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Environment
        {
            bool envEnabled = ctx.document.environment.has_value();
            if (ImGui::Checkbox("Environment##envchk", &envEnabled)) {
                ctx.pushUndo();
                if (envEnabled)
                    ctx.document.environment = Mc3::Mc3Environment{};
                else
                    ctx.document.environment.reset();
                ctx.markModified();
            }
            if (ctx.document.environment) {
                auto& env = *ctx.document.environment;

                // Background color
                // STAB-0719: found missing pushUndo() during a systematic
                // undo-coverage audit -- the adjacent BG Texture/Fog fields
                // in this same block already have it.
                ImGui::TextDisabled("Background");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh2142 = ImGui::ColorEdit3("##envbg", env.backgroundColor.data(),
                        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    if (_undoCh2142) {
                    ctx.markModified();
                } }

                // Background texture
                ImGui::TextDisabled("BG Texture");
                {
                    char tbuf[256];
                    std::strncpy(tbuf, env.backgroundTexture.c_str(), sizeof(tbuf)-1);
                    tbuf[255] = '\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##envbgtex", tbuf, sizeof(tbuf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        ctx.pushUndo();
                        env.backgroundTexture = tbuf;
                        ctx.markModified();
                    }
                }

                // Skybox texture (equirectangular panorama). F20 (2026-07-20
                // audit): this field existed in the "Env" left-panel tab
                // (MeshCraftApplication_UiLeftPanel.cpp) but was missing
                // here entirely -- the two editors share one
                // Mc3Environment, so a skybox set via the Env tab was
                // invisible/unreachable from this panel.
                ImGui::TextDisabled("Skybox Texture (equirect)");
                {
                    char sbuf[256];
                    std::strncpy(sbuf, env.skyboxTexture.c_str(), sizeof(sbuf)-1);
                    sbuf[255] = '\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##envskybox", sbuf, sizeof(sbuf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        ctx.pushUndo();
                        env.skyboxTexture = sbuf;
                        ctx.markModified();
                    }
                }

                // Fog
                {
                    bool fogEnabled = env.fog.has_value();
                    if (ImGui::Checkbox("Fog##fogchk", &fogEnabled)) {
                        ctx.pushUndo();
                        if (fogEnabled)
                            env.fog = Mc3::Mc3Fog{};
                        else
                            env.fog.reset();
                        ctx.markModified();
                    }
                    if (env.fog) {
                        auto& fog = *env.fog;
                        ImGui::Indent();

                        ImGui::TextDisabled("Color");
                        ImGui::SetNextItemWidth(-1);
                        { bool _undoCh2139 = ImGui::ColorEdit3("##fogcol", fog.color.data(), ImGuiColorEditFlags_Float);
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        if (_undoCh2139) {
                            ctx.markModified();
                        } }

                        const char* fogModes[] = {"Linear", "Exponential"};
                        int fogModeIdx = (fog.mode == Mc3::FogMode::Exponential) ? 1 : 0;
                        ImGui::TextDisabled("Mode");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Combo("##fogmode", &fogModeIdx, fogModes, 2)) {
                            ctx.pushUndo();
                            fog.mode = (fogModeIdx == 1) ? Mc3::FogMode::Exponential : Mc3::FogMode::Linear;
                            ctx.markModified();
                        }

                        if (fog.mode == Mc3::FogMode::Linear) {
                            ImGui::TextDisabled("Start");
                            ImGui::SetNextItemWidth(-1);
                            // AlwaysClamp: keeps start/end within their
                            // (mutually dynamic) bounds even against a
                            // Ctrl+Click typed value, consistent with the
                            // divide-by-zero guard already in SceneRenderer.
                            { bool _undoCh2161 = ImGui::DragFloat("##fogstart", &fog.start, 0.5f, 0.0f, fog.end, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            if (_undoCh2161) {
                                ctx.markModified();
                            } }
                            ImGui::TextDisabled("End");
                            ImGui::SetNextItemWidth(-1);
                            { bool _undoCh2167 = ImGui::DragFloat("##fogend", &fog.end, 0.5f, fog.start, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            if (_undoCh2167) {
                                ctx.markModified();
                            } }
                        } else {
                            ImGui::TextDisabled("Density");
                            ImGui::SetNextItemWidth(-1);
                            { bool _undoCh2174 = ImGui::DragFloat("##fogdens", &fog.density, 0.001f, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            if (_undoCh2174) {
                                ctx.markModified();
                            } }
                        }

                        ImGui::Unindent();
                    }
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Read-only stats
        ImGui::TextDisabled("Version  %s", ctx.document.version.c_str());

        // Count objects recursively
        int totalObjs = 0, totalVis = 0, totalLocked = 0;
        std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> countAll;
        countAll = [&](const auto& list) {
            for (const auto& o : list) {
                ++totalObjs;
                if (o->visible) ++totalVis;
                if (ctx.lockedIds.count(o->id)) ++totalLocked;
                countAll(o->children);
            }
        };
        countAll(ctx.document.objects);

        ImGui::TextDisabled("Objects  %d  (%d visible, %d locked)",
                            totalObjs, totalVis, totalLocked);
        if (!ctx.document.lights.empty())
            ImGui::TextDisabled("Lights   %d", static_cast<int>(ctx.document.lights.size()));
        if (!ctx.document.cameras.empty())
            ImGui::TextDisabled("Cameras  %d", static_cast<int>(ctx.document.cameras.size()));
        if (!ctx.document.materials.empty())
            ImGui::TextDisabled("Materials %d", static_cast<int>(ctx.document.materials.size()));
        if (!ctx.document.definitions.empty())
            ImGui::TextDisabled("Defs     %d", static_cast<int>(ctx.document.definitions.size()));
        if (!ctx.document.textures.empty())
            ImGui::TextDisabled("Textures %d", static_cast<int>(ctx.document.textures.size()));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Click an object to inspect it.");
        if (ImGui::Button("Select All", ImVec2(-1, 0))) {
            for (auto& o : ctx.document.objects) ctx.selection.select(o);
            ctx.updateTitle();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

} // namespace MeshCraft::Scene
