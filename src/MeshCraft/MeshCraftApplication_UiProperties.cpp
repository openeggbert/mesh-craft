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


void MeshCraftApplication::drawPropertiesPanel(float panelY, float panelH, int screenW, int screenH)
{
    (void)screenW; (void)screenH;
    // Right panel — Properties
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(screenW - kRightPanelW), panelY));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kRightPanelW), panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.20f, 1.0f));
    ImGui::Begin("Properties", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    if (selection_.hasSelection()) {
        auto& sel0 = selection_.selection().front();

        // Object name
        {
            char nameBuf[128];
            std::strncpy(nameBuf, sel0->name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf)-1] = '\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                pushUndo();
                sel0->name = nameBuf;
                modified_ = true;
                updateWindowTitle();
            }
        }

        ImGui::Spacing();

        // Multi-select helpers
        int selN = static_cast<int>(selection_.selection().size());
        const auto& selAll = selection_.selection();

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
        if (showTimeline_ && !currentActionName_.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("K##kpos"))
                insertAnimKeyframes(*sel0, {Mc3::AnimatedProperty::PositionX,
                                            Mc3::AnimatedProperty::PositionY,
                                            Mc3::AnimatedProperty::PositionZ});
        }
        {
            float pos[3] = { sel0->transform.position[0], sel0->transform.position[1], sel0->transform.position[2] };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat3("##pos", pos, 0.1f)) {
                if (ImGui::IsItemActivated()) pushUndo();
                float dp[3] = { pos[0]-sel0->transform.position[0],
                                 pos[1]-sel0->transform.position[1],
                                 pos[2]-sel0->transform.position[2] };
                for (const auto& s : selection_.selection()) {
                    if (lockedIds_.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i) s->transform.position[i] += dp[i];
                }
                modified_ = true; updateWindowTitle();
            }
            // World-space position (read-only, shown when object is parented)
            {
                auto wm = sceneRenderer_->computeObjectWorldMatrix(*sel0, document_);
                ImGui::TextDisabled("World: %.3f, %.3f, %.3f", wm.M41, wm.M42, wm.M43);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("World-space position (after parent transforms)");
            }
        }

        // Transform: rotation (delta applied to all selected)
        multiLabel("Rotation", !allMatchF3([](const Mc3::Mc3Object* o){ return o->transform.rotation; }));
        if (showTimeline_ && !currentActionName_.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("K##krot"))
                insertAnimKeyframes(*sel0, {Mc3::AnimatedProperty::RotationX,
                                            Mc3::AnimatedProperty::RotationY,
                                            Mc3::AnimatedProperty::RotationZ});
        }
        {
            float rot[3] = { sel0->transform.rotation[0], sel0->transform.rotation[1], sel0->transform.rotation[2] };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat3("##rot", rot, 0.5f)) {
                if (ImGui::IsItemActivated()) pushUndo();
                float dr[3] = { rot[0]-sel0->transform.rotation[0],
                                 rot[1]-sel0->transform.rotation[1],
                                 rot[2]-sel0->transform.rotation[2] };
                for (const auto& s : selection_.selection()) {
                    if (lockedIds_.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i) s->transform.rotation[i] += dr[i];
                }
                modified_ = true; updateWindowTitle();
            }
        }

        // Transform: scale (delta applied to all selected)
        multiLabel("Scale", !allMatchF3([](const Mc3::Mc3Object* o){ return o->transform.scale; }));
        if (showTimeline_ && !currentActionName_.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("K##kscl"))
                insertAnimKeyframes(*sel0, {Mc3::AnimatedProperty::ScaleX,
                                            Mc3::AnimatedProperty::ScaleY,
                                            Mc3::AnimatedProperty::ScaleZ});
        }
        {
            float scl[3] = { sel0->transform.scale[0], sel0->transform.scale[1], sel0->transform.scale[2] };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat3("##scl", scl, 0.01f, 0.001f, 100.0f)) {
                if (ImGui::IsItemActivated()) pushUndo();
                float ds[3] = { scl[0]-sel0->transform.scale[0],
                                 scl[1]-sel0->transform.scale[1],
                                 scl[2]-sel0->transform.scale[2] };
                for (const auto& s : selection_.selection()) {
                    if (lockedIds_.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i)
                        s->transform.scale[i] = std::max(0.001f, s->transform.scale[i] + ds[i]);
                }
                modified_ = true; updateWindowTitle();
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
            } else if (selection_.selection().size() >= 2) {
                // Multi-selection: span of pivot positions
                float mn[3] = { 1e30f,  1e30f,  1e30f};
                float mx[3] = {-1e30f, -1e30f, -1e30f};
                for (const auto& s : selection_.selection()) {
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
        multiLabel("Pivot", !allMatchF3([](const Mc3::Mc3Object* o){ return o->transform.pivot; }));
        {
            float piv[3] = { sel0->transform.pivot[0], sel0->transform.pivot[1], sel0->transform.pivot[2] };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat3("##piv", piv, 0.1f)) {
                if (ImGui::IsItemActivated()) pushUndo();
                float dp[3] = { piv[0]-sel0->transform.pivot[0],
                                 piv[1]-sel0->transform.pivot[1],
                                 piv[2]-sel0->transform.pivot[2] };
                for (const auto& s : selAll) {
                    if (lockedIds_.count(s->id)) continue;
                    for (int i = 0; i < 3; ++i) s->transform.pivot[i] += dp[i];
                }
                modified_ = true; updateWindowTitle();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Visible
        {
            bool visMixed = !allMatchBool([](const Mc3::Mc3Object* o){ return o->visible; });
            bool vis = sel0->visible;
            if (ImGui::Checkbox("Visible", &vis)) {
                pushUndo();
                for (const auto& s : selAll) s->visible = vis;
                modified_ = true; updateWindowTitle();
            }
            if (visMixed) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f,0.75f,0.2f,1.0f),"~");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Values differ across selection");
            }
            if (showTimeline_ && !currentActionName_.empty()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("K##kvis"))
                    insertAnimKeyframes(*sel0, {Mc3::AnimatedProperty::Visible});
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
            for (const auto& [k, _] : document_.materials) matKeys.push_back(k);
            int curIdx = 0;
            for (int i = 1; i < (int)matKeys.size(); ++i)
                if (matKeys[i] == sel0->material) { curIdx = i; break; }
            ImGui::SetNextItemWidth(-1);
            const char* matTopPreview = matTopMixed ? "(mixed)" : (curIdx == 0 ? "(none)" : sel0->material.c_str());
            if (ImGui::BeginCombo("##matsel0", matTopPreview)) {
                for (int i = 0; i < (int)matKeys.size(); ++i) {
                    bool isSel = (!matTopMixed && i == curIdx);
                    if (i > 0 && document_.materials.count(matKeys[i])) {
                        const auto& m = document_.materials.at(matKeys[i]);
                        ImVec4 c(m.baseColor[0], m.baseColor[1], m.baseColor[2], 1.0f);
                        ImGui::ColorButton("##mcb", c,
                            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                            ImVec2(12, 12));
                        ImGui::SameLine(0, 4);
                    }
                    if (ImGui::Selectable(matKeys[i].c_str(), isSel)) {
                        pushUndo();
                        const std::string newMat = (i == 0) ? "" : matKeys[i];
                        for (const auto& s : selAll) s->material = newMat;
                        modified_ = true;
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (document_.materials.empty())
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
                pushUndo();
                for (const auto& s : selAll) s->collision = colOpts[colIdx];
                modified_ = true;
            }
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
                pushUndo();
                sel0->tags.erase(sel0->tags.begin() + tagToRemove);
                modified_ = true; updateWindowTitle();
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
                        pushUndo();
                        sel0->tags.push_back(trimmed);
                        modified_ = true; updateWindowTitle();
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
                    pushUndo();
                    if (st.position) sel0->transform.position = *st.position;
                    if (st.rotation) sel0->transform.rotation = *st.rotation;
                    if (st.scale)    sel0->transform.scale    = *st.scale;
                    if (st.visible.has_value()) sel0->visible = *st.visible;
                    if (st.material) sel0->material = *st.material;
                    modified_ = true; updateWindowTitle();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply this state to the object now");
                ImGui::SameLine();
                if (ImGui::SmallButton("Cap##stcp")) {
                    pushUndo();
                    if (st.position) st.position = sel0->transform.position;
                    if (st.rotation) st.rotation = sel0->transform.rotation;
                    if (st.scale)    st.scale    = sel0->transform.scale;
                    if (st.visible.has_value()) st.visible = sel0->visible;
                    if (st.material) st.material = sel0->material;
                    modified_ = true;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Re-capture from current object values");
                ImGui::SameLine();
                if (ImGui::SmallButton("Del##stdel")) {
                    pushUndo(); toDelete = stId; modified_ = true; updateWindowTitle();
                }

                if (open) {
                    ImGui::Indent();

                    // ── Position ────────────────────────────────────────────
                    {
                        bool hasP = st.position.has_value();
                        if (ImGui::Checkbox("Position##stp", &hasP)) {
                            pushUndo();
                            if (hasP) st.position = sel0->transform.position;
                            else      st.position.reset();
                            modified_ = true;
                        }
                        if (st.position) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::DragFloat3("##stpv", st.position->data(), 0.01f))
                                { pushUndo(); modified_ = true; }
                        }
                    }

                    // ── Rotation ────────────────────────────────────────────
                    {
                        bool hasR = st.rotation.has_value();
                        if (ImGui::Checkbox("Rotation##str", &hasR)) {
                            pushUndo();
                            if (hasR) st.rotation = sel0->transform.rotation;
                            else      st.rotation.reset();
                            modified_ = true;
                        }
                        if (st.rotation) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::DragFloat3("##strv", st.rotation->data(), 0.5f))
                                { pushUndo(); modified_ = true; }
                        }
                    }

                    // ── Scale ───────────────────────────────────────────────
                    {
                        bool hasS = st.scale.has_value();
                        if (ImGui::Checkbox("Scale##sts", &hasS)) {
                            pushUndo();
                            if (hasS) st.scale = sel0->transform.scale;
                            else      st.scale.reset();
                            modified_ = true;
                        }
                        if (st.scale) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::DragFloat3("##stsv", st.scale->data(), 0.01f, 0.001f, 1000.f))
                                { pushUndo(); modified_ = true; }
                        }
                    }

                    // ── Visible ─────────────────────────────────────────────
                    {
                        bool hasVis = st.visible.has_value();
                        if (ImGui::Checkbox("Visible##stv", &hasVis)) {
                            pushUndo();
                            if (hasVis) st.visible = sel0->visible;
                            else        st.visible.reset();
                            modified_ = true;
                        }
                        if (st.visible.has_value()) {
                            ImGui::SameLine();
                            bool v = *st.visible;
                            if (ImGui::Checkbox("##stvv", &v)) {
                                pushUndo(); st.visible = v; modified_ = true;
                            }
                        }
                    }

                    // ── Material ────────────────────────────────────────────
                    {
                        bool hasMat = st.material.has_value();
                        if (ImGui::Checkbox("Material##stm", &hasMat)) {
                            pushUndo();
                            if (hasMat) st.material = sel0->material;
                            else        st.material.reset();
                            modified_ = true;
                        }
                        if (st.material) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1);
                            // Combo from document materials
                            const std::string& cur = *st.material;
                            if (ImGui::BeginCombo("##stmv", cur.empty() ? "(none)" : cur.c_str())) {
                                if (ImGui::Selectable("(none)", cur.empty())) {
                                    pushUndo(); st.material = std::string{}; modified_ = true;
                                }
                                for (const auto& [mk, _] : document_.materials) {
                                    bool sel = (mk == cur);
                                    if (ImGui::Selectable(mk.c_str(), sel)) {
                                        pushUndo(); *st.material = mk; modified_ = true;
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
                pushUndo();
                states[newStateName] = Mc3::Mc3ObjectState{};
                modified_ = true; updateWindowTitle();
                newStateName[0] = '\0';
            }
            if (!nameOk) ImGui::EndDisabled();
            ImGui::SameLine();
            if (!nameOk) ImGui::BeginDisabled();
            if (ImGui::SmallButton("+ Capture")) {
                pushUndo();
                Mc3::Mc3ObjectState st;
                st.position = sel0->transform.position;
                st.rotation = sel0->transform.rotation;
                st.scale    = sel0->transform.scale;
                st.visible  = sel0->visible;
                if (!sel0->material.empty()) st.material = sel0->material;
                states[newStateName] = st;
                modified_ = true; updateWindowTitle();
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
                pushUndo();
                csg.csgType = static_cast<Mc3::CsgType>(csgIdx);
                sel0->type  = (csgIdx == 0) ? Mc3::ObjectType::Union
                            : (csgIdx == 1) ? Mc3::ObjectType::Difference
                            : Mc3::ObjectType::Intersection;
                modified_ = true; updateWindowTitle();
            }

            if (sel0->type == Mc3::ObjectType::Difference && !sel0->children.empty()) {
                ImGui::Spacing();
                ImGui::TextDisabled("Children — check = cutter");
                for (auto& child : sel0->children) {
                    ImGui::PushID(child->id.c_str());
                    bool isCut = child->isCutter;
                    const std::string& cname = child->name.empty() ? child->id : child->name;
                    if (ImGui::Checkbox(cname.c_str(), &isCut)) {
                        pushUndo();
                        child->isCutter = isCut;
                        modified_ = true; updateWindowTitle();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mark as cutter volume (subtracted from base)");
                    ImGui::PopID();
                }
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
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.size[0] = std::max(0.001f, sz[0]);
                    p.size[1] = std::max(0.001f, sz[1]);
                    p.size[2] = std::max(0.001f, sz[2]);
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::Sphere: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##prad", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.radius = std::max(0.001f, r);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##psegs", &segs, 4, 64)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.segments = segs;
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::Cylinder:
            case Mc3::PrimitiveType::Cone: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##prad", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.radius = std::max(0.001f, r);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Height");
                float h = p.height;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##phgt", &h, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.height = std::max(0.001f, h);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##psegs", &segs, 4, 64)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.segments = segs;
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::Plane: {
                ImGui::TextDisabled("Width");
                float w = p.size[0];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ppw", &w, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.size[0] = std::max(0.001f, w);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Depth");
                float d = p.size[2];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ppd", &d, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.size[2] = std::max(0.001f, d);
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::Disk: {
                ImGui::TextDisabled("Outer Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pdsk_r", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.radius = std::max(0.001f, r);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Inner Radius (0 = solid)");
                float ir = p.minorRadius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pdsk_ir", &ir, 0.01f, 0.0f, p.radius - 0.001f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.minorRadius = std::clamp(ir, 0.0f, p.radius - 0.001f);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##pdsk_segs", &segs, 3, 128)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.segments = segs;
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::Capsule: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pcap_r", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.radius = std::max(0.001f, r);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Height (cylinder part)");
                float h = p.height;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pcap_h", &h, 0.01f, 0.0f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.height = std::max(0.0f, h);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##pcap_segs", &segs, 6, 64)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.segments = segs;
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::Grid: {
                ImGui::TextDisabled("Width (X)");
                float sx = p.size[0];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pgrd_sx", &sx, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.size[0] = std::max(0.001f, sx);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Depth (Z)");
                float sz = p.size[2];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pgrd_sz", &sz, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.size[2] = std::max(0.001f, sz);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Subdivisions X");
                int subX = p.subdivisionsX;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##pgrd_subx", &subX, 1, 64)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.subdivisionsX = std::max(1, subX);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Subdivisions Z");
                int subZ = p.subdivisionsZ;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##pgrd_subz", &subZ, 1, 64)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.subdivisionsZ = std::max(1, subZ);
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::IcoSphere: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##pico_r", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.radius = std::max(0.001f, r);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("320 triangles (2 subdivisions)");
                break;
            }
            case Mc3::PrimitiveType::Torus: {
                ImGui::TextDisabled("Major Radius");
                float mr = p.majorRadius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ptor_mr", &mr, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.majorRadius = std::max(0.001f, mr);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Minor Radius");
                float rr = p.minorRadius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ptor_rr", &rr, 0.01f, 0.001f, p.majorRadius)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.minorRadius = std::clamp(rr, 0.001f, p.majorRadius);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##ptor_segs", &segs, 4, 64)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.segments = segs;
                    modified_ = true; updateWindowTitle();
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
                if (ImGui::IsItemActivated()) pushUndo();
                modified_ = true; updateWindowTitle();
            }
            ImGui::TextDisabled("Path Segments");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderInt("##exsegs", &ex.segments, 1, 128)) {
                if (ImGui::IsItemActivated()) pushUndo();
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::Checkbox("Smooth", &ex.smooth))  { modified_ = true; updateWindowTitle(); }
            ImGui::SameLine();
            if (ImGui::Checkbox("Caps",   &ex.caps))    { modified_ = true; updateWindowTitle(); }

            // --- Cross-section ---
            if (ImGui::TreeNodeEx("Cross-section", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& cs = ex.crossSection;
                const char* csTypes[] = { "Rect", "Circle", "Polygon", "Custom", "Star" };
                int csIdx = static_cast<int>(cs.type);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##cstype", &csIdx, csTypes, 5)) {
                    pushUndo();
                    cs.type = static_cast<Mc3::CrossSectionType>(csIdx);
                    modified_ = true; updateWindowTitle();
                }
                switch (cs.type) {
                case Mc3::CrossSectionType::Rect:
                    ImGui::TextDisabled("Width");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csw", &cs.width, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        cs.width = std::max(0.001f, cs.width);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Height");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csh", &cs.height, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        cs.height = std::max(0.001f, cs.height);
                        modified_ = true; updateWindowTitle();
                    }
                    break;
                case Mc3::CrossSectionType::Circle:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csr", &cs.radius, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        cs.radius = std::max(0.001f, cs.radius);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Inner Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csir", &cs.innerRadius, 0.01f, 0.0f, cs.radius)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        cs.innerRadius = std::max(0.0f, cs.innerRadius);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Segments");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##csseg", &cs.segments, 3, 64)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                    break;
                case Mc3::CrossSectionType::Polygon:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##cspr", &cs.radius, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        cs.radius = std::max(0.001f, cs.radius);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Inner Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##cspir", &cs.innerRadius, 0.01f, 0.0f, cs.radius)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        cs.innerRadius = std::max(0.0f, cs.innerRadius);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Sides");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##cspsd", &cs.sides, 3, 32)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                    break;
                case Mc3::CrossSectionType::Star:
                    ImGui::TextDisabled("Points (tips)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##csstpts", &cs.sides, 3, 16)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        cs.sides = std::max(3, cs.sides);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Outer Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csstr", &cs.radius, 0.01f, 0.001f, 1000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        cs.radius = std::max(0.001f, cs.radius);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Inner Radius (0 = 50%)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##csstir", &cs.innerRadius, 0.01f, 0.0f, cs.radius)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        cs.innerRadius = std::clamp(cs.innerRadius, 0.0f, cs.radius);
                        modified_ = true; updateWindowTitle();
                    }
                    break;
                case Mc3::CrossSectionType::Custom:
                    ImGui::TextDisabled("Points (X Y)");
                    for (int pi = 0; pi < static_cast<int>(cs.customPoints.size()); ++pi) {
                        ImGui::PushID(pi);
                        float xy[2] = { cs.customPoints[pi].x, cs.customPoints[pi].y };
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20);
                        if (ImGui::DragFloat2("##cpt", xy, 0.01f)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            cs.customPoints[pi].x = xy[0];
                            cs.customPoints[pi].y = xy[1];
                            modified_ = true; updateWindowTitle();
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("x")) {
                            pushUndo();
                            cs.customPoints.erase(cs.customPoints.begin() + pi);
                            modified_ = true; updateWindowTitle();
                            ImGui::PopID(); break;
                        }
                        ImGui::PopID();
                    }
                    if (ImGui::SmallButton("+ Point")) {
                        pushUndo();
                        cs.customPoints.push_back({0.0f, 0.0f});
                        modified_ = true; updateWindowTitle();
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
                    pushUndo();
                    path.type = static_cast<Mc3::ExtrudePathType>(ptIdx);
                    modified_ = true; updateWindowTitle();
                }
                switch (path.type) {
                case Mc3::ExtrudePathType::Line: {
                    ImGui::TextDisabled("Length");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##plen", &path.length, 0.01f, 0.001f, 10000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        path.length = std::max(0.001f, path.length);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Axis");
                    const char* axes[] = { "x", "y", "z" };
                    int axIdx = (path.axis == "x") ? 0 : (path.axis == "z") ? 2 : 1;
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##paxis", &axIdx, axes, 3)) {
                        pushUndo();
                        path.axis = axes[axIdx];
                        modified_ = true; updateWindowTitle();
                    }
                    break;
                }
                case Mc3::ExtrudePathType::Arc:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##parr", &path.arcRadius, 0.01f, 0.001f, 10000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        path.arcRadius = std::max(0.001f, path.arcRadius);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Angle (deg)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##para", &path.arcAngle, 1.0f, -360.0f, 360.0f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                    break;
                case Mc3::ExtrudePathType::Helix:
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##phr", &path.helixRadius, 0.01f, 0.001f, 10000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        path.helixRadius = std::max(0.001f, path.helixRadius);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Height");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##phh", &path.helixHeight, 0.01f, 0.001f, 10000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        path.helixHeight = std::max(0.001f, path.helixHeight);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Turns");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##pht", &path.helixTurns, 0.1f, 0.1f, 1000.f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        path.helixTurns = std::max(0.1f, path.helixTurns);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Pitch (height per turn)");
                    ImGui::SetNextItemWidth(-1);
                    {
                        float pitch = (path.helixTurns > 0.0f)
                            ? path.helixHeight / path.helixTurns : 0.0f;
                        if (ImGui::DragFloat("##php", &pitch, 0.01f, 0.001f, 10000.f)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            pitch = std::max(0.001f, pitch);
                            path.helixHeight = pitch * path.helixTurns;
                            modified_ = true; updateWindowTitle();
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
                            if (ImGui::IsItemActivated()) pushUndo();
                            modified_ = true; updateWindowTitle();
                        }
                        if (isBez) {
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20);
                            if (ImGui::DragFloat3("##pc", pt.controlIn.data(), 0.1f)) {
                                if (ImGui::IsItemActivated()) pushUndo();
                                modified_ = true; updateWindowTitle();
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("x")) {
                            pushUndo();
                            path.points.erase(path.points.begin() + pi);
                            modified_ = true; updateWindowTitle();
                            ImGui::PopID(); break;
                        }
                        ImGui::PopID();
                    }
                    if (ImGui::SmallButton("+ Point")) {
                        pushUndo();
                        Mc3::Mc3PathPoint pp;
                        if (!path.points.empty()) pp.position = path.points.back().position;
                        path.points.push_back(pp);
                        modified_ = true; updateWindowTitle();
                    }
                    break;
                }
                }
                ImGui::TreePop();
            }
        }

        // Mesh source
        if (sel0->type == Mc3::ObjectType::Mesh) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Mesh Source");
            char srcBuf[512];
            std::strncpy(srcBuf, sel0->meshSource.c_str(), sizeof(srcBuf)-1); srcBuf[511]='\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##meshsrc", srcBuf, sizeof(srcBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                pushUndo(); sel0->meshSource = srcBuf; modified_ = true; updateWindowTitle();
            }
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
                    pushUndo(); sel0->definition = ""; modified_ = true; updateWindowTitle();
                }
                for (const auto& [defId, _] : document_.definitions) {
                    bool selected = (defId == sel0->definition);
                    if (ImGui::Selectable(defId.c_str(), selected)) {
                        pushUndo(); sel0->definition = defId; modified_ = true; updateWindowTitle();
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // Definition content preview
            if (!sel0->definition.empty()) {
                auto defIt = document_.definitions.find(sel0->definition);
                if (defIt != document_.definitions.end() && defIt->second) {
                    const auto& defRoot = *defIt->second;
                    // Count total nodes (root + all descendants)
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
                    pushUndo(); vref = vbuf; modified_ = true; updateWindowTitle();
                }
                ImGui::SameLine(0, 4);
                if (ImGui::SmallButton("x")) {
                    pushUndo();
                    sel0->variantDefinitions.erase(sel0->variantDefinitions.begin() + vi);
                    modified_ = true; updateWindowTitle();
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
                    pushUndo();
                    sel0->variantDefinitions.emplace_back(addVarBuf);
                    addVarBuf[0] = '\0';
                    modified_ = true; updateWindowTitle();
                }
                // Quick-add from existing definitions
                if (ImGui::BeginCombo("##vardefpick", nullptr, ImGuiComboFlags_NoPreview)) {
                    for (const auto& [defId, _] : document_.definitions) {
                        if (ImGui::Selectable(defId.c_str())) {
                            pushUndo();
                            sel0->variantDefinitions.push_back(defId);
                            modified_ = true; updateWindowTitle();
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
                pushUndo(); sel0->materialOverride = moBuf; modified_ = true; updateWindowTitle();
            }
        }

        // Deform (geometry-level non-uniform scale)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            bool deformEnabled = sel0->deform.has_value();
            if (ImGui::Checkbox("Deform", &deformEnabled)) {
                pushUndo();
                if (deformEnabled) sel0->deform = Mc3::Mc3Deform{};
                else               sel0->deform.reset();
                modified_ = true; updateWindowTitle();
            }
            if (sel0->deform.has_value()) {
                auto& d = *sel0->deform;
                ImGui::TextDisabled("Deform Scale");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat3("##deform", d.scale.data(), 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    d.scale[0] = std::max(0.001f, d.scale[0]);
                    d.scale[1] = std::max(0.001f, d.scale[1]);
                    d.scale[2] = std::max(0.001f, d.scale[2]);
                    modified_ = true; updateWindowTitle();
                }
            }
        }

        // Poly stats (C6)
        {
            int v = 0, t = 0;
            sceneRenderer_->objectPolyStats(*sel0, v, t);
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
                        pushUndo();
                        for (const auto& s : selAll) s->material = "";
                        modified_ = true; updateWindowTitle();
                    }
                    for (const auto& [key, _] : document_.materials) {
                        bool selected = (!matEdMixed && key == sel0->material);
                        if (ImGui::Selectable(key.c_str(), selected)) {
                            pushUndo();
                            for (const auto& s : selAll) s->material = key;
                            modified_ = true; updateWindowTitle();
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("New")) {
                    pushUndo();
                    // Generate unique key
                    int n = 1;
                    std::string key;
                    do { key = "material_" + std::to_string(n++); }
                    while (document_.materials.count(key));
                    Mc3::Mc3Material newMat;
                    newMat.name = key;
                    document_.materials[key] = newMat;
                    sel0->material = key;
                    modified_ = true; updateWindowTitle();
                }
            }

            // Edit the assigned material's fields inline
            auto matIt = document_.materials.find(sel0->material);
            if (matIt != document_.materials.end()) {
                auto& mat = matIt->second;

                // Base color
                ImGui::TextDisabled("Base Color");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit4("##mbc", mat.baseColor.data(),
                        ImGuiColorEditFlags_NoLabel)) {
                    modified_ = true; updateWindowTitle();
                }

                // Roughness
                ImGui::TextDisabled("Roughness");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##mrough", &mat.roughness, 0.0f, 1.0f)) {
                    modified_ = true; updateWindowTitle();
                }

                // Metallic
                ImGui::TextDisabled("Metallic");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##mmetal", &mat.metallic, 0.0f, 1.0f)) {
                    modified_ = true; updateWindowTitle();
                }

                // Emissive color
                ImGui::TextDisabled("Emissive");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##memit", mat.emissiveColor.data(),
                        ImGuiColorEditFlags_NoLabel)) {
                    modified_ = true; updateWindowTitle();
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
                    modified_ = true; updateWindowTitle();
                }
                if (mat.alphaMode == "mask") {
                    ImGui::TextDisabled("Alpha Cutoff");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##mcut", &mat.alphaCutoff, 0.0f, 1.0f)) {
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Double sided
                if (ImGui::Checkbox("Double Sided", &mat.doubleSided)) {
                    modified_ = true; updateWindowTitle();
                }

                // Normal scale + occlusion strength (collapsed by default)
                if (ImGui::TreeNode("Advanced")) {
                    ImGui::TextDisabled("Normal Scale");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##mnrmscl", &mat.normalScale, 0.01f, 0.0f, 10.0f)) {
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Occlusion Strength");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##moccstr", &mat.occlusionStrength, 0.01f, 0.0f, 1.0f)) {
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TreePop();
                }

                // Textures (collapsed by default)
                if (ImGui::TreeNode("Textures")) {
                    auto texField = [&](const char* label, std::string& field) {
                        ImGui::TextDisabled("%s", label);
                        char buf[256];
                        std::strncpy(buf, field.c_str(), sizeof(buf) - 1); buf[255] = '\0';
                        ImGui::SetNextItemWidth(-1);
                        std::string id = std::string("##t") + label;
                        if (ImGui::InputText(id.c_str(), buf, sizeof(buf),
                                ImGuiInputTextFlags_EnterReturnsTrue)) {
                            field = buf; modified_ = true; updateWindowTitle();
                        }
                    };
                    texField("Base Color",      mat.baseColorTexture);
                    texField("Normal",          mat.normalTexture);
                    texField("Emissive",        mat.emissiveTexture);
                    texField("Metal/Roughness", mat.metallicRoughnessTexture);
                    texField("Occlusion",       mat.occlusionTexture);
                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndTabItem();
        } // end Material tab

        if (ImGui::BeginTabItem("Anim")) {
        if (!showTimeline_ || currentActionName_.empty()) {
            ImGui::TextDisabled("No action selected.");
            ImGui::TextDisabled("Open Timeline and select an animation action.");
        } else {
            using AP = Mc3::AnimatedProperty;

            // Helper: check if this object has a channel for the given property in the current action
            const auto& act = document_.actions.at(currentActionName_);
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
                insertAnimKeyframes(*sel0, {
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
                insertAnimKeyframes(*sel0, {AP::PositionX, AP::PositionY, AP::PositionZ});
            ImGui::SameLine();
            { bool a = hasChan(AP::PositionX);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KX##px")) insertAnimKeyframes(*sel0, {AP::PositionX});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::PositionY);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KY##py")) insertAnimKeyframes(*sel0, {AP::PositionY});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::PositionZ);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KZ##pz")) insertAnimKeyframes(*sel0, {AP::PositionZ});
              if (a) ImGui::PopStyleColor(); }

            // Rotation group + per-axis
            dot({AP::RotationX, AP::RotationY, AP::RotationZ});
            if (ImGui::SmallButton("K Rot"))
                insertAnimKeyframes(*sel0, {AP::RotationX, AP::RotationY, AP::RotationZ});
            ImGui::SameLine();
            { bool a = hasChan(AP::RotationX);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KX##rx")) insertAnimKeyframes(*sel0, {AP::RotationX});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::RotationY);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KY##ry")) insertAnimKeyframes(*sel0, {AP::RotationY});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::RotationZ);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KZ##rz")) insertAnimKeyframes(*sel0, {AP::RotationZ});
              if (a) ImGui::PopStyleColor(); }

            // Scale group + per-axis
            dot({AP::ScaleX, AP::ScaleY, AP::ScaleZ});
            if (ImGui::SmallButton("K Scl"))
                insertAnimKeyframes(*sel0, {AP::ScaleX, AP::ScaleY, AP::ScaleZ});
            ImGui::SameLine();
            { bool a = hasChan(AP::ScaleX);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KX##sx")) insertAnimKeyframes(*sel0, {AP::ScaleX});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::ScaleY);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KY##sy")) insertAnimKeyframes(*sel0, {AP::ScaleY});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::ScaleZ);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KZ##sz")) insertAnimKeyframes(*sel0, {AP::ScaleZ});
              if (a) ImGui::PopStyleColor(); }

            // Visibility
            dot({AP::Visible});
            if (ImGui::SmallButton("K Vis"))
                insertAnimKeyframes(*sel0, {AP::Visible});

            ImGui::Spacing();
            ImGui::Separator();

            // ── Deform ─────────────────────────────────────────────────────
            ImGui::TextDisabled("Deform");
            dot({AP::DeformX, AP::DeformY, AP::DeformZ});
            if (ImGui::SmallButton("K Deform"))
                insertAnimKeyframes(*sel0, {AP::DeformX, AP::DeformY, AP::DeformZ});
            ImGui::SameLine();
            { bool a = hasChan(AP::DeformX);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KX##dx")) insertAnimKeyframes(*sel0, {AP::DeformX});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::DeformY);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KY##dy")) insertAnimKeyframes(*sel0, {AP::DeformY});
              if (a) ImGui::PopStyleColor(); }
            ImGui::SameLine();
            { bool a = hasChan(AP::DeformZ);
              if (a) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.45f,0.2f,1.f));
              if (ImGui::SmallButton("KZ##dz")) insertAnimKeyframes(*sel0, {AP::DeformZ});
              if (a) ImGui::PopStyleColor(); }

            ImGui::Spacing();
            ImGui::Separator();

            // ── Material ───────────────────────────────────────────────────
            ImGui::TextDisabled("Material");
            dot({AP::MaterialBaseColorR, AP::MaterialBaseColorG,
                 AP::MaterialBaseColorB, AP::MaterialBaseColorA});
            if (ImGui::SmallButton("K Color"))
                insertAnimKeyframes(*sel0, {AP::MaterialBaseColorR, AP::MaterialBaseColorG,
                                            AP::MaterialBaseColorB, AP::MaterialBaseColorA});
            ImGui::SameLine();
            dot({AP::MaterialEmissiveR, AP::MaterialEmissiveG, AP::MaterialEmissiveB});
            if (ImGui::SmallButton("K Emit"))
                insertAnimKeyframes(*sel0, {AP::MaterialEmissiveR, AP::MaterialEmissiveG,
                                            AP::MaterialEmissiveB});
            dot({AP::MaterialRoughness});
            if (ImGui::SmallButton("K Rough"))
                insertAnimKeyframes(*sel0, {AP::MaterialRoughness});
            ImGui::SameLine();
            dot({AP::MaterialMetallic});
            if (ImGui::SmallButton("K Metal"))
                insertAnimKeyframes(*sel0, {AP::MaterialMetallic});
        }
        ImGui::EndTabItem();
        } // end Anim tab

        if (ImGui::BeginTabItem("UV")) {
        bool hasUV = sel0->uvMapping.has_value();
        if (ImGui::Checkbox("Enable UV mapping##uven", &hasUV)) {
            pushUndo();
            if (hasUV) sel0->uvMapping = Mc3::Mc3UvMapping{};
            else       sel0->uvMapping.reset();
            modified_ = true;
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
                pushUndo();
                m.projection = static_cast<Mc3::UvProjection>(proj);
                modified_ = true;
            }
            ImGui::Spacing();
            ImGui::Text("Scale  U/V:");
            float sc[2] = {m.scaleU, m.scaleV};
            ImGui::SetNextItemWidth(130.0f);
            if (ImGui::DragFloat2("##uvsc", sc, 0.01f, 0.001f, 100.0f, "%.3f")) {
                pushUndo(); m.scaleU = sc[0]; m.scaleV = sc[1]; modified_ = true;
            }
            ImGui::Text("Offset U/V:");
            float of[2] = {m.offsetU, m.offsetV};
            ImGui::SetNextItemWidth(130.0f);
            if (ImGui::DragFloat2("##uvof", of, 0.005f, -100.0f, 100.0f, "%.3f")) {
                pushUndo(); m.offsetU = of[0]; m.offsetV = of[1]; modified_ = true;
            }
            ImGui::Text("Rotation:  ");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::DragFloat("##uvrot", &m.rotation, 0.5f, -360.0f, 360.0f, "%.1f\xc2\xb0")) {
                pushUndo(); modified_ = true;
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
            std::strncpy(nameBuf, document_.model.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf)-1] = '\0';
            ImGui::TextDisabled("Name");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##scname", nameBuf, sizeof(nameBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                pushUndo();
                document_.model = nameBuf;
                modified_ = true;
                updateWindowTitle();
            }
        }

        // Unit
        {
            const char* unitOpts[] = {
                "meter", "centimeter", "millimeter", "inch", "foot"
            };
            int unitIdx = 0;
            for (int i = 0; i < 5; ++i)
                if (document_.unit == unitOpts[i]) { unitIdx = i; break; }
            ImGui::TextDisabled("Unit");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##scunit", &unitIdx, unitOpts, 5)) {
                pushUndo();
                document_.unit = unitOpts[unitIdx];
                modified_ = true;
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
                if (document_.coordinateSystem == csOpts[i]) { csIdx = i; break; }
            ImGui::TextDisabled("Coord System");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##sccs", &csIdx, csOpts, 3)) {
                pushUndo();
                document_.coordinateSystem = csOpts[csIdx];
                modified_ = true;
            }
        }

        // Default camera
        if (!document_.cameras.empty()) {
            ImGui::TextDisabled("Default Camera");
            const char* dcPrev = document_.defaultCamera.empty()
                ? "(none)" : document_.defaultCamera.c_str();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##scdc", dcPrev)) {
                if (ImGui::Selectable("(none)", document_.defaultCamera.empty()))
                    document_.defaultCamera = "";
                for (const auto& cam : document_.cameras) {
                    bool sel = (cam.name == document_.defaultCamera);
                    if (ImGui::Selectable(cam.name.c_str(), sel))
                        document_.defaultCamera = cam.name;
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
            bool envEnabled = document_.environment.has_value();
            if (ImGui::Checkbox("Environment##envchk", &envEnabled)) {
                pushUndo();
                if (envEnabled)
                    document_.environment = Mc3::Mc3Environment{};
                else
                    document_.environment.reset();
                modified_ = true;
            }
            if (document_.environment) {
                auto& env = *document_.environment;

                // Background color
                ImGui::TextDisabled("Background");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##envbg", env.backgroundColor.data(),
                        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)) {
                    modified_ = true;
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
                        pushUndo();
                        env.backgroundTexture = tbuf;
                        modified_ = true;
                    }
                }

                // Fog
                {
                    bool fogEnabled = env.fog.has_value();
                    if (ImGui::Checkbox("Fog##fogchk", &fogEnabled)) {
                        pushUndo();
                        if (fogEnabled)
                            env.fog = Mc3::Mc3Fog{};
                        else
                            env.fog.reset();
                        modified_ = true;
                    }
                    if (env.fog) {
                        auto& fog = *env.fog;
                        ImGui::Indent();

                        ImGui::TextDisabled("Color");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::ColorEdit3("##fogcol", fog.color.data(), ImGuiColorEditFlags_Float)) {
                            modified_ = true;
                        }

                        const char* fogModes[] = {"Linear", "Exponential"};
                        int fogModeIdx = (fog.mode == Mc3::FogMode::Exponential) ? 1 : 0;
                        ImGui::TextDisabled("Mode");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Combo("##fogmode", &fogModeIdx, fogModes, 2)) {
                            pushUndo();
                            fog.mode = (fogModeIdx == 1) ? Mc3::FogMode::Exponential : Mc3::FogMode::Linear;
                            modified_ = true;
                        }

                        if (fog.mode == Mc3::FogMode::Linear) {
                            ImGui::TextDisabled("Start");
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::DragFloat("##fogstart", &fog.start, 0.5f, 0.0f, fog.end))
                                modified_ = true;
                            ImGui::TextDisabled("End");
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::DragFloat("##fogend", &fog.end, 0.5f, fog.start, 10000.0f))
                                modified_ = true;
                        } else {
                            ImGui::TextDisabled("Density");
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::DragFloat("##fogdens", &fog.density, 0.001f, 0.0f, 1.0f, "%.4f"))
                                modified_ = true;
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
        ImGui::TextDisabled("Version  %s", document_.version.c_str());

        // Count objects recursively
        int totalObjs = 0, totalVis = 0, totalLocked = 0;
        std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> countAll;
        countAll = [&](const auto& list) {
            for (const auto& o : list) {
                ++totalObjs;
                if (o->visible) ++totalVis;
                if (lockedIds_.count(o->id)) ++totalLocked;
                countAll(o->children);
            }
        };
        countAll(document_.objects);

        ImGui::TextDisabled("Objects  %d  (%d visible, %d locked)",
                            totalObjs, totalVis, totalLocked);
        if (!document_.lights.empty())
            ImGui::TextDisabled("Lights   %d", static_cast<int>(document_.lights.size()));
        if (!document_.cameras.empty())
            ImGui::TextDisabled("Cameras  %d", static_cast<int>(document_.cameras.size()));
        if (!document_.materials.empty())
            ImGui::TextDisabled("Materials %d", static_cast<int>(document_.materials.size()));
        if (!document_.definitions.empty())
            ImGui::TextDisabled("Defs     %d", static_cast<int>(document_.definitions.size()));
        if (!document_.textures.empty())
            ImGui::TextDisabled("Textures %d", static_cast<int>(document_.textures.size()));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Click an object to inspect it.");
        if (ImGui::Button("Select All", ImVec2(-1, 0))) {
            for (auto& o : document_.objects) selection_.select(o);
            updateWindowTitle();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();

}


} // namespace MeshCraft
