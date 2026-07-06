#include "MeshCraft/Scene/PropertiesPanel.hpp"
#include "MeshCraft/Renderer/SceneRenderer.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"

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

namespace MeshCraft::Scene {

using namespace Microsoft::Xna::Framework;

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
            if (ImGui::DragFloat3("##pos", pos, 0.1f)) {
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                float dp[3] = { pos[0]-sel0->transform.position[0],
                                 pos[1]-sel0->transform.position[1],
                                 pos[2]-sel0->transform.position[2] };
                for (const auto& s : ctx.selection.selection()) {
                    if (ctx.lockedIds.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i) s->transform.position[i] += dp[i];
                }
                ctx.markModified();
            }
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
            if (ImGui::DragFloat3("##rot", rot, 0.5f)) {
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                float dr[3] = { rot[0]-sel0->transform.rotation[0],
                                 rot[1]-sel0->transform.rotation[1],
                                 rot[2]-sel0->transform.rotation[2] };
                for (const auto& s : ctx.selection.selection()) {
                    if (ctx.lockedIds.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i) s->transform.rotation[i] += dr[i];
                }
                ctx.markModified();
            }
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
            if (ImGui::DragFloat3("##scl", scl, 0.01f, 0.001f, 100.0f)) {
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                float ds[3] = { scl[0]-sel0->transform.scale[0],
                                 scl[1]-sel0->transform.scale[1],
                                 scl[2]-sel0->transform.scale[2] };
                for (const auto& s : ctx.selection.selection()) {
                    if (ctx.lockedIds.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i)
                        s->transform.scale[i] = std::max(0.001f, s->transform.scale[i] + ds[i]);
                }
                ctx.markModified();
            }
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
            if (ImGui::DragFloat3("##piv", piv, 0.1f)) {
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                float dp[3] = { piv[0]-sel0->transform.pivot[0],
                                 piv[1]-sel0->transform.pivot[1],
                                 piv[2]-sel0->transform.pivot[2] };
                for (const auto& s : selAll) {
                    if (ctx.lockedIds.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i) s->transform.pivot[i] += dp[i];
                }
                ctx.markModified();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset##piv", ImVec2(-1,0))) {
                ctx.pushUndo();
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

        // Collision
        {
            bool colMixed = !allMatchStr([](const Mc3::Mc3Object* o){ return o->collision; });
            ImGui::TextDisabled("Collision");
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
                            if (ImGui::DragFloat3("##stpv", st.position->data(), 0.01f))
                                { ctx.pushUndo(); ctx.markModified(); }
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
                            if (ImGui::DragFloat3("##strv", st.rotation->data(), 0.5f))
                                { ctx.pushUndo(); ctx.markModified(); }
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
                            if (ImGui::DragFloat3("##stsv", st.scale->data(), 0.01f, 0.001f, 1000.f))
                                { ctx.pushUndo(); ctx.markModified(); }
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

                // K3: Export CSG result as OBJ
                ImGui::Spacing();
                if (ImGui::Button("Export OBJ…")) {
                    ctx.openCsgExport(sel0.get());
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Build CSG result and export as Wavefront OBJ");
            }
        }

        // Geometry parameters
        if (sel0->primitive) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Geometry");

            auto& p = *sel0->primitive;

            switch (p.primitiveType) {
            case Mc3::PrimitiveType::Box:
            case Mc3::PrimitiveType::Cube: {
                float sz[3] = { p.size[0], p.size[1], p.size[2] };
                ImGui::TextDisabled("Size (W/H/D)");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat3("##psize", sz, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.size[0] = std::max(0.001f, sz[0]);
                    p.size[1] = std::max(0.001f, sz[1]);
                    p.size[2] = std::max(0.001f, sz[2]);
                    ctx.markModified();
                }
                break;
            }
            case Mc3::PrimitiveType::Sphere: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##prad", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##psegs", &segs, 4, 64)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.segments = segs;
                    ctx.markModified();
                }
                break;
            }
            case Mc3::PrimitiveType::Cylinder:
            case Mc3::PrimitiveType::Cone: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##prad", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Height");
                float h = p.height;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##phgt", &h, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.height = std::max(0.001f, h);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##psegs", &segs, 4, 64)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.segments = segs;
                    ctx.markModified();
                }
                break;
            }
            case Mc3::PrimitiveType::Plane: {
                ImGui::TextDisabled("Width");
                float w = p.size[0];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ppw", &w, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.size[0] = std::max(0.001f, w);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Depth");
                float d = p.size[2];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ppd", &d, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.size[2] = std::max(0.001f, d);
                    ctx.markModified();
                }
                break;
            }
            case Mc3::PrimitiveType::Disk: {
                ImGui::TextDisabled("Outer Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pdsk_r", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Inner Radius (0 = solid)");
                float ir = p.minorRadius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pdsk_ir", &ir, 0.01f, 0.0f, p.radius - 0.001f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.minorRadius = std::clamp(ir, 0.0f, p.radius - 0.001f);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##pdsk_segs", &segs, 3, 128)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.segments = segs;
                    ctx.markModified();
                }
                break;
            }
            case Mc3::PrimitiveType::Capsule: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pcap_r", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Height (cylinder part)");
                float h = p.height;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pcap_h", &h, 0.01f, 0.0f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.height = std::max(0.0f, h);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##pcap_segs", &segs, 6, 64)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.segments = segs;
                    ctx.markModified();
                }
                break;
            }
            case Mc3::PrimitiveType::Grid: {
                ImGui::TextDisabled("Width (X)");
                float sx = p.size[0];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pgrd_sx", &sx, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.size[0] = std::max(0.001f, sx);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Depth (Z)");
                float sz = p.size[2];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pgrd_sz", &sz, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.size[2] = std::max(0.001f, sz);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Subdivisions X");
                int subX = p.subdivisionsX;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##pgrd_subx", &subX, 1, 64)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.subdivisionsX = std::max(1, subX);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Subdivisions Z");
                int subZ = p.subdivisionsZ;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##pgrd_subz", &subZ, 1, 64)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.subdivisionsZ = std::max(1, subZ);
                    ctx.markModified();
                }
                break;
            }
            case Mc3::PrimitiveType::IcoSphere: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pico_r", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.radius = std::max(0.001f, r);
                    ctx.markModified();
                }
                ImGui::TextDisabled("320 triangles (2 subdivisions)");
                break;
            }
            case Mc3::PrimitiveType::Torus: {
                ImGui::TextDisabled("Major Radius");
                float mr = p.majorRadius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ptor_mr", &mr, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.majorRadius = std::max(0.001f, mr);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Minor Radius");
                float rr = p.minorRadius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ptor_rr", &rr, 0.01f, 0.001f, p.majorRadius)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.minorRadius = std::clamp(rr, 0.001f, p.majorRadius);
                    ctx.markModified();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##ptor_segs", &segs, 4, 64)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    p.segments = segs;
                    ctx.markModified();
                }
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
            if (ImGui::DragFloat("##extwist", &ex.twist, 1.0f, -3600.0f, 3600.0f)) {
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                ctx.markModified();
            }
            ImGui::TextDisabled("Path Segments");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderInt("##exsegs", &ex.segments, 1, 128)) {
                if (ImGui::IsItemActivated()) ctx.pushUndo();
                ctx.markModified();
            }
            if (ImGui::Checkbox("Smooth", &ex.smooth))  { ctx.markModified(); }
            ImGui::SameLine();
            if (ImGui::Checkbox("Caps",   &ex.caps))    { ctx.markModified(); }

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
                    if (ImGui::DragFloat("##csw", &cs.width, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        cs.width = std::max(0.001f, cs.width);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Height");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csh", &cs.height, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        cs.height = std::max(0.001f, cs.height);
                        ctx.markModified();
                    }
                    break;
                case Mc3::CrossSectionType::Circle:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csr", &cs.radius, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        cs.radius = std::max(0.001f, cs.radius);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Inner Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csir", &cs.innerRadius, 0.01f, 0.0f, cs.radius)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        cs.innerRadius = std::max(0.0f, cs.innerRadius);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Segments");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##csseg", &cs.segments, 3, 64)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        ctx.markModified();
                    }
                    break;
                case Mc3::CrossSectionType::Polygon:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##cspr", &cs.radius, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        cs.radius = std::max(0.001f, cs.radius);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Inner Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##cspir", &cs.innerRadius, 0.01f, 0.0f, cs.radius)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        cs.innerRadius = std::max(0.0f, cs.innerRadius);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Sides");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##cspsd", &cs.sides, 3, 32)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        ctx.markModified();
                    }
                    break;
                case Mc3::CrossSectionType::Star:
                    ImGui::TextDisabled("Points (tips)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##csstpts", &cs.sides, 3, 16)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        cs.sides = std::max(3, cs.sides);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Outer Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csstr", &cs.radius, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        cs.radius = std::max(0.001f, cs.radius);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Inner Radius (0 = 50%%)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csstir", &cs.innerRadius, 0.01f, 0.0f, cs.radius)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        cs.innerRadius = std::clamp(cs.innerRadius, 0.0f, cs.radius);
                        ctx.markModified();
                    }
                    break;
                case Mc3::CrossSectionType::Custom:
                    ImGui::TextDisabled("Points (X Y)");
                    for (int pi = 0; pi < static_cast<int>(cs.customPoints.size()); ++pi) {
                        ImGui::PushID(pi);
                        float xy[2] = { cs.customPoints[pi].x, cs.customPoints[pi].y };
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20);
                        if (ImGui::DragFloat2("##cpt", xy, 0.01f)) {
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            cs.customPoints[pi].x = xy[0];
                            cs.customPoints[pi].y = xy[1];
                            ctx.markModified();
                        }
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
                    if (ImGui::DragFloat("##plen", &path.length, 0.01f, 0.001f, 10000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        path.length = std::max(0.001f, path.length);
                        ctx.markModified();
                    }
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
                    if (ImGui::DragFloat("##parr", &path.arcRadius, 0.01f, 0.001f, 10000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        path.arcRadius = std::max(0.001f, path.arcRadius);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Angle (deg)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##para", &path.arcAngle, 1.0f, -360.0f, 360.0f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        ctx.markModified();
                    }
                    break;
                case Mc3::ExtrudePathType::Helix:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##phr", &path.helixRadius, 0.01f, 0.001f, 10000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        path.helixRadius = std::max(0.001f, path.helixRadius);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Height");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##phh", &path.helixHeight, 0.01f, 0.001f, 10000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        path.helixHeight = std::max(0.001f, path.helixHeight);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Turns");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##pht", &path.helixTurns, 0.1f, 0.1f, 1000.f)) {
                        if (ImGui::IsItemActivated()) ctx.pushUndo();
                        path.helixTurns = std::max(0.1f, path.helixTurns);
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Pitch (height per turn)");
                    ImGui::SetNextItemWidth(-1);
                    {
                        float pitch = (path.helixTurns > 0.0f)
                            ? path.helixHeight / path.helixTurns : 0.0f;
                        if (ImGui::DragFloat("##php", &pitch, 0.01f, 0.001f, 10000.f)) {
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            pitch = std::max(0.001f, pitch);
                            path.helixHeight = pitch * path.helixTurns;
                            ctx.markModified();
                        }
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
                        if (ImGui::DragFloat3("##pp", pt.position.data(), 0.1f)) {
                            if (ImGui::IsItemActivated()) ctx.pushUndo();
                            ctx.markModified();
                        }
                        if (isBez) {
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20);
                            if (ImGui::DragFloat3("##pc", pt.controlIn.data(), 0.1f)) {
                                if (ImGui::IsItemActivated()) ctx.pushUndo();
                                ctx.markModified();
                            }
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
                if (ImGui::DragFloat3("##deform", d.scale.data(), 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) ctx.pushUndo();
                    d.scale[0] = std::max(0.001f, d.scale[0]);
                    d.scale[1] = std::max(0.001f, d.scale[1]);
                    d.scale[2] = std::max(0.001f, d.scale[2]);
                    ctx.markModified();
                }
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
            auto matIt = ctx.document.materials.find(sel0->material);
            if (matIt != ctx.document.materials.end()) {
                auto& mat = matIt->second;

                // Base color
                ImGui::TextDisabled("Base Color");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit4("##mbc", mat.baseColor.data(),
                        ImGuiColorEditFlags_NoLabel)) {
                    ctx.markModified();
                }

                // Roughness
                ImGui::TextDisabled("Roughness");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: without it, Ctrl+Click lets a typed value go
                // out of [0,1], which would export a spec-invalid glTF
                // pbrMetallicRoughness.roughnessFactor with no other guard.
                if (ImGui::SliderFloat("##mrough", &mat.roughness, 0.0f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_AlwaysClamp)) {
                    ctx.markModified();
                }

                // Metallic
                ImGui::TextDisabled("Metallic");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: same out-of-[0,1]-via-Ctrl+Click risk as
                // roughness above (spec-invalid metallicFactor on export).
                if (ImGui::SliderFloat("##mmetal", &mat.metallic, 0.0f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_AlwaysClamp)) {
                    ctx.markModified();
                }

                // Emissive color
                ImGui::TextDisabled("Emissive");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##memit", mat.emissiveColor.data(),
                        ImGuiColorEditFlags_NoLabel)) {
                    ctx.markModified();
                }

                // Alpha mode
                ImGui::TextDisabled("Alpha Mode");
                ImGui::SetNextItemWidth(-1);
                const char* alphaModes[] = { "opaque", "mask", "blend" };
                int alphaIdx = 0;
                for (int i = 0; i < 3; ++i)
                    if (mat.alphaMode == alphaModes[i]) { alphaIdx = i; break; }
                if (ImGui::Combo("##malpha", &alphaIdx, alphaModes, 3)) {
                    mat.alphaMode = alphaModes[alphaIdx];
                    ctx.markModified();
                }
                if (mat.alphaMode == "mask") {
                    ImGui::TextDisabled("Alpha Cutoff");
                    ImGui::SetNextItemWidth(-1);
                    // AlwaysClamp: same out-of-[0,1]-via-Ctrl+Click risk as
                    // roughness/metallic above (spec-invalid alphaCutoff).
                    if (ImGui::SliderFloat("##mcut", &mat.alphaCutoff, 0.0f, 1.0f, "%.3f",
                                           ImGuiSliderFlags_AlwaysClamp)) {
                        ctx.markModified();
                    }
                }

                // Double sided
                if (ImGui::Checkbox("Double Sided", &mat.doubleSided)) {
                    ctx.markModified();
                }

                // Normal scale + occlusion strength (collapsed by default)
                if (ImGui::TreeNode("Advanced")) {
                    ImGui::TextDisabled("Normal Scale");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##mnrmscl", &mat.normalScale, 0.01f, 0.0f, 10.0f)) {
                        ctx.markModified();
                    }
                    ImGui::TextDisabled("Occlusion Strength");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##moccstr", &mat.occlusionStrength, 0.01f, 0.0f, 1.0f)) {
                        ctx.markModified();
                    }
                    ImGui::TreePop();
                }

                // Textures (collapsed by default)
                if (ImGui::TreeNode("Textures")) {
                    bool hasPendingTex = !ctx.pendingDropTexture.empty();
                    // texField: tracks hover for OS drag-drop (D6)
                    auto texField = [&](const char* label, std::string& field, const char* slot) {
                        ImGui::TextDisabled("%s", label);
                        char buf[512];
                        std::strncpy(buf, field.c_str(), sizeof(buf) - 1); buf[511] = '\0';
                        ImGui::SetNextItemWidth(-1);
                        // Highlight field that is the current drop target
                        bool isHov = (ctx.hoveredTexSlot == slot && ctx.hoveredTexMatId == ctx.selectedMaterialKey);
                        if (isHov && hasPendingTex)
                            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.5f, 0.2f, 0.6f));
                        std::string id = std::string("##t") + label;
                        if (ImGui::InputText(id.c_str(), buf, sizeof(buf),
                                ImGuiInputTextFlags_EnterReturnsTrue)) {
                            field = buf; ctx.markModified();
                        }
                        if (isHov && hasPendingTex) ImGui::PopStyleColor();
                        if (ImGui::IsItemHovered()) {
                            ctx.hoveredTexSlot  = slot;
                            ctx.hoveredTexMatId = ctx.selectedMaterialKey;
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
            if (ImGui::DragFloat2("##uvsc", sc, 0.01f, 0.001f, 100.0f, "%.3f")) {
                ctx.pushUndo(); m.scaleU = sc[0]; m.scaleV = sc[1]; ctx.markModified();
            }
            ImGui::Text("Offset U/V:");
            float of[2] = {m.offsetU, m.offsetV};
            ImGui::SetNextItemWidth(130.0f);
            if (ImGui::DragFloat2("##uvof", of, 0.005f, -100.0f, 100.0f, "%.3f")) {
                ctx.pushUndo(); m.offsetU = of[0]; m.offsetV = of[1]; ctx.markModified();
            }
            ImGui::Text("Rotation:  ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::DragFloat("##uvrot", &m.rotation, 0.5f, -360.0f, 360.0f, "%.1f\xc2\xb0")) {
                ctx.pushUndo(); ctx.markModified();
            }
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

        // Coordinate system
        {
            const char* csOpts[] = {
                "right_handed_y_up",
                "right_handed_z_up",
                "left_handed_y_up"
            };
            int csIdx = 0;
            for (int i = 0; i < 3; ++i)
                if (ctx.document.coordinateSystem == csOpts[i]) { csIdx = i; break; }
            ImGui::TextDisabled("Coord System");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##sccs", &csIdx, csOpts, 3)) {
                ctx.pushUndo();
                ctx.document.coordinateSystem = csOpts[csIdx];
                ctx.markModified();
            }
        }

        // Default camera
        if (!ctx.document.cameras.empty()) {
            ImGui::TextDisabled("Default Camera");
            const char* dcPrev = ctx.document.defaultCamera.empty()
                ? "(none)" : ctx.document.defaultCamera.c_str();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##scdc", dcPrev)) {
                if (ImGui::Selectable("(none)", ctx.document.defaultCamera.empty()))
                    ctx.document.defaultCamera = "";
                for (const auto& cam : ctx.document.cameras) {
                    bool sel = (cam.name == ctx.document.defaultCamera);
                    if (ImGui::Selectable(cam.name.c_str(), sel))
                        ctx.document.defaultCamera = cam.name;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
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
                ImGui::TextDisabled("Background");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##envbg", env.backgroundColor.data(),
                        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)) {
                    ctx.markModified();
                }

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
                        if (ImGui::ColorEdit3("##fogcol", fog.color.data(), ImGuiColorEditFlags_Float)) {
                            ctx.markModified();
                        }

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
                            if (ImGui::DragFloat("##fogstart", &fog.start, 0.5f, 0.0f, fog.end))
                                ctx.markModified();
                            ImGui::TextDisabled("End");
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::DragFloat("##fogend", &fog.end, 0.5f, fog.start, 10000.0f))
                                ctx.markModified();
                        } else {
                            ImGui::TextDisabled("Density");
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::DragFloat("##fogdens", &fog.density, 0.001f, 0.0f, 1.0f, "%.4f"))
                                ctx.markModified();
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
