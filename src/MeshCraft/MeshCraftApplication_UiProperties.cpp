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
                const char* csTypes[] = { "Rect", "Circle", "Polygon", "Custom" };
                int csIdx = static_cast<int>(cs.type);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##cstype", &csIdx, csTypes, 4)) {
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
