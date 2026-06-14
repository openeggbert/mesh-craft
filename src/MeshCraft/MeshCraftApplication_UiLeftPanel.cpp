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


void MeshCraftApplication::drawLeftPanel(float panelY, float panelH)
{
    int tlPanelH = showTimeline_ ? kTimelineH : 0;
    (void)tlPanelH;
    // -----------------------------------------------------------------------
    // Left panel — tabbed (Scene / Lights)
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(0, panelY));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kLeftPanelW), panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.20f, 1.0f));
    ImGui::Begin("##leftpanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::BeginTabBar("##lefttabs")) {

        // -------------------------------------------------------------------
        // Tab: Scene hierarchy
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Scene")) {
            // --- Drag-and-drop helpers (all operate on the full document_ tree) ---
            std::function<std::shared_ptr<Mc3::Mc3Object>(
                const std::vector<std::shared_ptr<Mc3::Mc3Object>>&,
                const std::string&)> findObj;
            findObj = [&](const auto& list, const std::string& id)
                    -> std::shared_ptr<Mc3::Mc3Object> {
                for (const auto& o : list) {
                    if (o->id == id) return o;
                    if (auto f = findObj(o->children, id)) return f;
                }
                return nullptr;
            };

            std::function<std::shared_ptr<Mc3::Mc3Object>(
                std::vector<std::shared_ptr<Mc3::Mc3Object>>&,
                const std::string&)> detachObj;
            detachObj = [&](auto& list, const std::string& id)
                    -> std::shared_ptr<Mc3::Mc3Object> {
                for (auto it = list.begin(); it != list.end(); ++it) {
                    if ((*it)->id == id) { auto r = *it; list.erase(it); return r; }
                    if (auto f = detachObj((*it)->children, id)) return f;
                }
                return nullptr;
            };

            // Returns true if targetId is 'root' itself or any descendant
            std::function<bool(const Mc3::Mc3Object&, const std::string&)> inSubtree;
            inSubtree = [&](const Mc3::Mc3Object& root, const std::string& targetId) -> bool {
                if (root.id == targetId) return true;
                for (const auto& c : root.children)
                    if (inSubtree(*c, targetId)) return true;
                return false;
            };

            auto doReparent = [&](const std::string& dragId,
                                  std::shared_ptr<Mc3::Mc3Object> newParent) {
                auto dragged = findObj(document_.objects, dragId);
                if (!dragged) return;
                if (newParent && (newParent->id == dragId || inSubtree(*dragged, newParent->id)))
                    return; // would create cycle
                pushUndo();
                detachObj(document_.objects, dragId);
                if (newParent) newParent->children.push_back(dragged);
                else           document_.objects.push_back(dragged);
                modified_ = true;
                updateWindowTitle();
            };

            // --- Search filter + expand/collapse all ---
            float btnW = 22.0f;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btnW * 2 - ImGui::GetStyle().ItemSpacing.x * 2);
            ImGui::InputTextWithHint("##hfilter", "Search...", hierarchyFilter_, sizeof(hierarchyFilter_));
            if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                hierarchyFilter_[0] = '\0';
            ImGui::SameLine();
            bool expandAll   = ImGui::SmallButton("+##ea");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Expand All");
            ImGui::SameLine();
            bool collapseAll = ImGui::SmallButton("-##ca");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Collapse All");

            // --- Type filter bar (E5) ---
            {
                struct TypeBtn { const char* label; int id; const char* tip; };
                static const TypeBtn kBtns[] = {
                    {"All",  0, "Show all objects"},
                    {"Prim", 1, "Primitives (box, sphere, cylinder…)"},
                    {"Mesh", 2, "External mesh objects"},
                    {"Grp",  3, "Group nodes"},
                    {"Inst", 4, "Instances (prefab references)"},
                    {"CSG",  5, "CSG operations (union, difference, intersection)"},
                    {"Ext",  6, "Extrude objects"},
                };
                for (const auto& b : kBtns) {
                    bool active = (hierTypeFilter_ == b.id);
                    if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
                    if (ImGui::SmallButton(b.label)) hierTypeFilter_ = b.id;
                    if (active) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", b.tip);
                    ImGui::SameLine();
                }
                ImGui::NewLine();
            }

            // --- Layer filter (E7) ---
            {
                // Collect all layer names used in the document
                std::set<std::string> layerNames;
                std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> collectLayers;
                collectLayers = [&](const auto& list) {
                    for (const auto& o : list) {
                        if (!o->layer.empty()) layerNames.insert(o->layer);
                        collectLayers(o->children);
                    }
                };
                collectLayers(document_.objects);

                if (!layerNames.empty()) {
                    ImGui::Text("Layer:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 28.0f);
                    const std::string& curLay = hierLayerFilter_;
                    if (ImGui::BeginCombo("##layfilter",
                                          curLay.empty() ? "(all layers)" : curLay.c_str())) {
                        if (ImGui::Selectable("(all layers)", curLay.empty()))
                            hierLayerFilter_.clear();
                        for (const auto& ln : layerNames) {
                            bool sel = (ln == curLay);
                            if (ImGui::Selectable(ln.c_str(), sel)) hierLayerFilter_ = ln;
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (!hierLayerFilter_.empty()) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("×##layclr")) hierLayerFilter_.clear();
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear layer filter");
                    }
                }
            }
            ImGui::Separator();

            // Build lowercase filter string once
            std::string filterLower = hierarchyFilter_;
            for (auto& ch : filterLower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            bool filtering = !filterLower.empty();
            bool typeFiltering  = (hierTypeFilter_ != 0);
            bool layerFiltering = !hierLayerFilter_.empty();

            // Returns true if obj's type matches the selected type filter
            auto matchesType = [&](const Mc3::Mc3Object& o) -> bool {
                if (!typeFiltering) return true;
                using OT = Mc3::ObjectType;
                switch (hierTypeFilter_) {
                case 1: // Prim
                    return o.type == OT::Box || o.type == OT::Cube || o.type == OT::Sphere ||
                           o.type == OT::Cylinder || o.type == OT::Cone || o.type == OT::Plane ||
                           o.type == OT::Torus || o.type == OT::Capsule || o.type == OT::Disk ||
                           o.type == OT::Grid || o.type == OT::IcoSphere;
                case 2: return o.type == OT::Mesh;
                case 3: return o.type == OT::Group || o.type == OT::Area;
                case 4: return o.type == OT::Instance;
                case 5: return o.type == OT::Union || o.type == OT::Difference ||
                               o.type == OT::Intersection;
                case 6: return o.type == OT::Extrude;
                default: return true;
                }
            };

            // Returns true if obj itself or any descendant matches all active filters
            std::function<bool(const Mc3::Mc3Object&)> matchesFilter;
            matchesFilter = [&](const Mc3::Mc3Object& o) -> bool {
                // text filter
                if (filtering) {
                    std::string nl = o.name.empty() ? o.id : o.name;
                    for (auto& ch : nl) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    bool textOk = nl.find(filterLower) != std::string::npos;
                    if (!textOk) {
                        for (const auto& c : o.children) if (matchesFilter(*c)) return true;
                        return false;
                    }
                }
                // type filter
                if (typeFiltering) {
                    if (!matchesType(o)) {
                        for (const auto& c : o.children) if (matchesFilter(*c)) return true;
                        return false;
                    }
                }
                // layer filter
                if (layerFiltering) {
                    if (o.layer != hierLayerFilter_) {
                        for (const auto& c : o.children) if (matchesFilter(*c)) return true;
                        return false;
                    }
                }
                return true;
            };

            // Rebuild flat order for shift-click range selection
            hierarchyFlatOrder_.clear();
            std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> buildFlat;
            buildFlat = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
                for (const auto& o : list) {
                    hierarchyFlatOrder_.push_back(o);
                    buildFlat(o->children);
                }
            };
            buildFlat(document_.objects);

            // --- Hierarchy draw ---
            std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> drawHierarchy;
            drawHierarchy = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
                for (const auto& obj : list) {
                    if (filtering && !matchesFilter(*obj)) continue;
                    ImGui::PushID(obj.get()); // pointer-stable; avoids conflicts when id is empty
                    bool sel = selection_.isSelected(obj.get());
                    bool hasChildren = !obj->children.empty();
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                               ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (filtering && hasChildren) flags |= ImGuiTreeNodeFlags_DefaultOpen;
                    if (sel)          flags |= ImGuiTreeNodeFlags_Selected;
                    // Expand / collapse all: override open state for this frame
                    if (expandAll   && hasChildren) ImGui::SetNextItemOpen(true,  ImGuiCond_Always);
                    if (collapseAll && hasChildren) ImGui::SetNextItemOpen(false, ImGuiCond_Always);

                    ImVec4 nodeColor = obj->visible ? ImVec4(1,1,1,1) : ImVec4(0.5f,0.5f,0.5f,1);
                    const char* typePrefix = "";
                    if      (obj->type == Mc3::ObjectType::Union)        { typePrefix = "[U] "; nodeColor = obj->visible ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.2f,0.45f,0.2f,1); }
                    else if (obj->type == Mc3::ObjectType::Difference)   { typePrefix = "[D] "; nodeColor = obj->visible ? ImVec4(0.9f,0.3f,0.3f,1) : ImVec4(0.45f,0.2f,0.2f,1); }
                    else if (obj->type == Mc3::ObjectType::Intersection) { typePrefix = "[X] "; nodeColor = obj->visible ? ImVec4(0.3f,0.6f,1.0f,1) : ImVec4(0.2f,0.35f,0.5f,1); }
                    else if (obj->type == Mc3::ObjectType::Group)        { typePrefix = "[G] "; }
                    else if (obj->type == Mc3::ObjectType::Box ||
                             obj->type == Mc3::ObjectType::Cube)         { typePrefix = "[B] "; }
                    else if (obj->type == Mc3::ObjectType::Sphere)       { typePrefix = "[S] "; }
                    else if (obj->type == Mc3::ObjectType::Cylinder)     { typePrefix = "[C] "; }
                    else if (obj->type == Mc3::ObjectType::Cone)         { typePrefix = "[K] "; }
                    else if (obj->type == Mc3::ObjectType::Plane)        { typePrefix = "[P] "; }
                    else if (obj->type == Mc3::ObjectType::Torus)        { typePrefix = "[T] "; }
                    else if (obj->type == Mc3::ObjectType::Capsule)      { typePrefix = "[Q] "; }
                    else if (obj->type == Mc3::ObjectType::Disk)         { typePrefix = "[O] "; }
                    else if (obj->type == Mc3::ObjectType::Grid)         { typePrefix = "[#] "; }
                    else if (obj->type == Mc3::ObjectType::IcoSphere)    { typePrefix = "[I] "; }
                    else if (obj->type == Mc3::ObjectType::Mesh)         { typePrefix = "[M] "; }
                    else if (obj->type == Mc3::ObjectType::Extrude)      { typePrefix = "[E] "; }
                    else if (obj->type == Mc3::ObjectType::Instance)     { typePrefix = "[i] "; }
                    else if (obj->type == Mc3::ObjectType::Area)         { typePrefix = "[A] "; }
                    else if (obj->isCutter)                              { typePrefix = "[cut] "; nodeColor = obj->visible ? ImVec4(1.0f,0.5f,0.3f,1) : ImVec4(0.5f,0.3f,0.2f,1); }

                    // Tag-based row color: hash first tag to a hue
                    if (!obj->tags.empty() &&
                        obj->type != Mc3::ObjectType::Union &&
                        obj->type != Mc3::ObjectType::Difference &&
                        obj->type != Mc3::ObjectType::Intersection &&
                        !obj->isCutter)
                    {
                        size_t h = std::hash<std::string>{}(obj->tags[0]);
                        float hue = static_cast<float>(h % 1000) / 1000.0f;
                        float r, g, b;
                        float sat = obj->visible ? 0.65f : 0.35f;
                        float val = obj->visible ? 0.95f : 0.55f;
                        ImGui::ColorConvertHSVtoRGB(hue, sat, val, r, g, b);
                        nodeColor = ImVec4(r, g, b, 1.0f);
                    }
                    bool isLocked = lockedIds_.count(obj->id) > 0;
                    std::string displayLabel = std::string(typePrefix) + (obj->name.empty() ? obj->id : obj->name);
                    if (!renamingId_.empty() && obj->id == renamingId_) {
                        // Inline rename: leaf node + InputText
                        ImGui::TreeNodeEx("##rn",
                            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                            ImGuiTreeNodeFlags_SpanAvailWidth);
                        ImGui::SameLine(0, 4);
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (renameNeedsFocus_) {
                            ImGui::SetKeyboardFocusHere();
                            renameNeedsFocus_ = false;
                        }
                        bool enter = ImGui::InputText("##ri", renameBuf_, sizeof(renameBuf_),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                        bool deact = ImGui::IsItemDeactivated();
                        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                            renamingId_.clear();
                        } else if (enter || deact) {
                            if (renameBuf_[0]) {
                                pushUndo();
                                obj->name = renameBuf_;
                                modified_ = true; updateWindowTitle();
                            }
                            renamingId_.clear();
                        }
                    } else {
                        // If command palette requested scroll-to this object, force open and scroll
                        if (!hierarchyScrollToId_.empty() && obj->id == hierarchyScrollToId_) {
                            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                            ImGui::SetScrollHereY(0.5f);
                            hierarchyScrollToId_.clear();
                        }

                        ImGui::PushStyleColor(ImGuiCol_Text, nodeColor);
                        ImGui::SetNextItemAllowOverlap();
                        bool nodeOpen = ImGui::TreeNodeEx(displayLabel.c_str(), flags);
                        ImGui::PopStyleColor();

                        // Lock tint: draw semi-transparent orange stripe over the row
                        if (isLocked) {
                            ImVec2 rMin = ImGui::GetItemRectMin();
                            ImVec2 rMax = ImGui::GetItemRectMax();
                            // Left-edge accent bar (4 px)
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                rMin, ImVec2(rMin.x + 4.0f, rMax.y),
                                IM_COL32(230, 140, 20, 200));
                            // Full-row semi-transparent tint
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                rMin, rMax, IM_COL32(230, 140, 20, 28));
                        }

                        // Drag source
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                            ImGui::SetDragDropPayload("MC3_OBJ", obj->id.c_str(), obj->id.size() + 1);
                            ImGui::Text("%s", displayLabel.c_str());
                            ImGui::EndDragDropSource();
                        }
                        // Drop target: dragged object becomes last child of this obj
                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("MC3_OBJ"))
                                doReparent(std::string(static_cast<const char*>(pl->Data)), obj);
                            ImGui::EndDragDropTarget();
                        }

                        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                            const bool ctrl  = ImGui::GetIO().KeyCtrl;
                            const bool shift = ImGui::GetIO().KeyShift;
                            if (shift && !hierarchyAnchorId_.empty()) {
                                // Range-select from anchor to here via flat order
                                auto findIdx = [&](const std::string& id) -> int {
                                    for (int i = 0; i < (int)hierarchyFlatOrder_.size(); ++i)
                                        if (hierarchyFlatOrder_[i]->id == id) return i;
                                    return -1;
                                };
                                int a = findIdx(hierarchyAnchorId_);
                                int b = findIdx(obj->id);
                                if (a >= 0 && b >= 0) {
                                    if (!ctrl) selection_.clear();
                                    if (a > b) std::swap(a, b);
                                    for (int i = a; i <= b; ++i)
                                        selection_.select(hierarchyFlatOrder_[i]);
                                }
                            } else if (ctrl) {
                                if (selection_.isSelected(obj.get())) selection_.deselect(obj);
                                else                                    selection_.select(obj);
                                hierarchyAnchorId_ = obj->id;
                            } else {
                                selection_.clear();
                                selection_.select(obj);
                                hierarchyAnchorId_ = obj->id;
                            }
                            updateWindowTitle();
                        }
                        // Double-click to rename
                        if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) {
                            renamingId_ = obj->id;
                            std::strncpy(renameBuf_, obj->name.c_str(), sizeof(renameBuf_) - 1);
                            renameBuf_[sizeof(renameBuf_) - 1] = '\0';
                            renameNeedsFocus_ = true;
                        }
                        if (ImGui::BeginPopupContextItem("##objctx")) {
                            if (ImGui::MenuItem("Select Parent\tP")) selectParent();
                            if (ImGui::MenuItem("Select Children", nullptr, false, !obj->children.empty()))
                                selectChildren();
                            ImGui::Separator();
                            if (ImGui::MenuItem("Rename")) {
                                renamingId_ = obj->id;
                                std::strncpy(renameBuf_, obj->name.c_str(), sizeof(renameBuf_) - 1);
                                renameBuf_[sizeof(renameBuf_) - 1] = '\0';
                                renameNeedsFocus_ = true;
                            }
                            if (ImGui::MenuItem("Batch Rename...\tCtrl+Shift+R")) batchRenameOpen_ = true;
                            if (ImGui::MenuItem("Duplicate")) duplicateSelected();
                            if (ImGui::MenuItem("Delete", nullptr, false, !isLocked)) deleteSelected();
                            ImGui::Separator();
                            if (ImGui::MenuItem("Group Selection")) {
                                pushUndo();
                                int gn = 1;
                                std::string gid;
                                do { gid = "group_" + std::to_string(gn++); }
                                while (findObj(document_.objects, gid) != nullptr);
                                auto grp = std::make_shared<Mc3::Mc3Object>();
                                grp->id   = gid;
                                grp->name = gid;
                                grp->type = Mc3::ObjectType::Group;
                                grp->transform.scale = {1.0f, 1.0f, 1.0f};
                                auto selCopy = selection_.selection();
                                for (const auto& s : selCopy) {
                                    detachObj(document_.objects, s->id);
                                    grp->children.push_back(s);
                                }
                                document_.objects.push_back(grp);
                                selection_.clear();
                                selection_.select(grp);
                                modified_ = true; updateWindowTitle();
                            }
                            if (ImGui::MenuItem("Set as Root")) {
                                doReparent(obj->id, nullptr);
                            }
                            ImGui::Separator();
                            if (ImGui::MenuItem(obj->visible ? "Hide" : "Show")) {
                                pushUndo(); obj->visible = !obj->visible; modified_ = true; updateWindowTitle();
                            }
                            if (ImGui::MenuItem(isLocked ? "Unlock\tCtrl+L" : "Lock\tCtrl+L")) {
                                if (isLocked) lockedIds_.erase(obj->id);
                                else          lockedIds_.insert(obj->id);
                            }
                            ImGui::EndPopup();
                        }

                        // Lock + visibility buttons — right-aligned in the row
                        {
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1,1));

                            // Lock button
                            float lockX = ImGui::GetWindowContentRegionMax().x - 36.0f;
                            ImGui::SameLine(lockX);
                            ImGui::PushStyleColor(ImGuiCol_Button,
                                isLocked ? ImVec4(0.70f,0.42f,0.05f,0.90f)
                                         : ImVec4(0.18f,0.18f,0.18f,0.45f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                isLocked ? ImVec4(0.90f,0.58f,0.10f,1.00f)
                                         : ImVec4(0.32f,0.32f,0.32f,0.65f));
                            ImGui::PushStyleColor(ImGuiCol_Text,
                                isLocked ? ImVec4(1.0f, 0.92f, 0.4f, 1.0f)
                                         : ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
                            if (ImGui::SmallButton(isLocked ? "\xe2\x96\xa0##lk" : "\xe2\x96\xa1##lk")) {
                                if (isLocked) lockedIds_.erase(obj->id);
                                else          lockedIds_.insert(obj->id);
                            }
                            ImGui::PopStyleColor(3);
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip(isLocked ? "Locked — click to unlock (Ctrl+L)" : "Click to lock (Ctrl+L)");

                            // Visibility button
                            float visX = ImGui::GetWindowContentRegionMax().x - 18.0f;
                            ImGui::SameLine(visX);
                            bool vis = obj->visible;
                            ImGui::PushStyleColor(ImGuiCol_Button,
                                vis ? ImVec4(0.10f,0.40f,0.10f,0.70f)
                                    : ImVec4(0.22f,0.22f,0.22f,0.50f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                vis ? ImVec4(0.20f,0.60f,0.20f,0.85f)
                                    : ImVec4(0.35f,0.35f,0.35f,0.70f));
                            if (ImGui::SmallButton(vis ? "v##vs" : "h##vs")) {
                                pushUndo();
                                obj->visible = !obj->visible;
                                modified_ = true; updateWindowTitle();
                            }
                            ImGui::PopStyleColor(2);
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip(vis ? "Hide" : "Show");

                            ImGui::PopStyleVar();
                        }

                        if (hasChildren && nodeOpen)
                            drawHierarchy(obj->children);
                        if (hasChildren && nodeOpen)
                            ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            };
            drawHierarchy(document_.objects);

            // Root-level drop zone: drag here to make object a top-level item
            ImGui::Dummy(ImVec2(-1.0f, 10.0f));
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("MC3_OBJ")) {
                    std::string dragId(static_cast<const char*>(pl->Data));
                    bool alreadyRoot = false;
                    for (const auto& o : document_.objects)
                        if (o->id == dragId) { alreadyRoot = true; break; }
                    if (!alreadyRoot) doReparent(dragId, nullptr);
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Lights
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Lights")) {
            // Clamp selection in case lights were deleted
            if (selectedLightIdx_ >= static_cast<int>(document_.lights.size()))
                selectedLightIdx_ = static_cast<int>(document_.lights.size()) - 1;

            // Toolbar: Add / Remove
            if (ImGui::SmallButton("+")) {
                pushUndo();
                Mc3::Mc3Light newLight;
                newLight.name = "Light " + std::to_string(document_.lights.size() + 1);
                document_.lights.push_back(newLight);
                selectedLightIdx_ = static_cast<int>(document_.lights.size()) - 1;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-") && selectedLightIdx_ >= 0) {
                pushUndo();
                document_.lights.erase(document_.lights.begin() + selectedLightIdx_);
                selectedLightIdx_ = std::min(selectedLightIdx_,
                    static_cast<int>(document_.lights.size()) - 1);
                modified_ = true; updateWindowTitle();
            }

            // Light list
            ImGui::Separator();
            for (int i = 0; i < static_cast<int>(document_.lights.size()); ++i) {
                const auto& li = document_.lights[i];
                const char* typeStr =
                    li.type == Mc3::LightType::Ambient      ? "[Amb]" :
                    li.type == Mc3::LightType::Directional  ? "[Dir]" :
                    li.type == Mc3::LightType::Spot         ? "[Spt]" : "[Pnt]";
                std::string label = std::string(typeStr) + " " +
                    (li.name.empty() ? ("light_" + std::to_string(i)) : li.name);
                ImGui::PushID(i);
                if (ImGui::Selectable(label.c_str(), selectedLightIdx_ == i))
                    selectedLightIdx_ = i;
                ImGui::PopID();
            }

            // Inline editor for selected light
            if (selectedLightIdx_ >= 0 && selectedLightIdx_ < static_cast<int>(document_.lights.size())) {
                auto& li = document_.lights[selectedLightIdx_];
                ImGui::Separator();
                ImGui::Spacing();

                // Name
                {
                    char buf[128];
                    std::strncpy(buf, li.name.c_str(), sizeof(buf)-1); buf[127]='\0';
                    ImGui::TextDisabled("Name");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##lname", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); li.name = buf; modified_ = true; updateWindowTitle();
                    }
                }

                // Type
                {
                    const char* types[] = { "Ambient", "Directional", "Spot", "Point" };
                    int tidx = static_cast<int>(li.type);
                    ImGui::TextDisabled("Type");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##ltype", &tidx, types, 4)) {
                        pushUndo(); li.type = static_cast<Mc3::LightType>(tidx);
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Color
                ImGui::TextDisabled("Color");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##lcol", li.color.data(),
                        ImGuiColorEditFlags_NoLabel)) {
                    modified_ = true; updateWindowTitle();
                }

                // Brightness
                ImGui::TextDisabled("Brightness");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##lbrt", &li.brightness, 0.01f, 0.0f, 100.0f)) {
                    modified_ = true; updateWindowTitle();
                }

                // Cast shadows
                if (ImGui::Checkbox("Cast Shadows", &li.castShadows)) {
                    modified_ = true; updateWindowTitle();
                }

                // Direction (Directional / Spot)
                if (li.type == Mc3::LightType::Directional || li.type == Mc3::LightType::Spot) {
                    ImGui::TextDisabled("Direction");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##ldir", li.direction.data(), 0.01f, -1.0f, 1.0f)) {
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Position (Spot / Point)
                if (li.type == Mc3::LightType::Spot || li.type == Mc3::LightType::Point) {
                    ImGui::TextDisabled("Position");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##lpos", li.position.data(), 0.1f)) {
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Range (0=unlimited)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##lrng", &li.range, 0.1f, 0.0f, 10000.0f)) {
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Spot-only params
                if (li.type == Mc3::LightType::Spot) {
                    ImGui::TextDisabled("Angle (deg)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##lang", &li.angle, 0.0f, 90.0f)) {
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Falloff");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##lfal", &li.falloff, 0.0f, 1.0f)) {
                        modified_ = true; updateWindowTitle();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Environment
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Env")) {
            // Enable / disable environment block
            bool envEnabled = document_.environment.has_value();
            if (ImGui::Checkbox("Enable", &envEnabled)) {
                pushUndo();
                if (envEnabled) document_.environment = Mc3::Mc3Environment{};
                else            document_.environment.reset();
                modified_ = true; updateWindowTitle();
            }

            if (document_.environment.has_value()) {
                auto& env = *document_.environment;

                ImGui::Spacing();

                // Background color
                ImGui::TextDisabled("Background Color");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##envbg", env.backgroundColor.data(),
                        ImGuiColorEditFlags_NoLabel)) {
                    modified_ = true; updateWindowTitle();
                }

                // Background texture
                ImGui::TextDisabled("Background Texture");
                {
                    char buf[256];
                    std::strncpy(buf, env.backgroundTexture.c_str(), sizeof(buf)-1); buf[255]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##envbgtex", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); env.backgroundTexture = buf;
                        modified_ = true; updateWindowTitle();
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Fog
                bool fogEnabled = env.fog.has_value();
                if (ImGui::Checkbox("Fog", &fogEnabled)) {
                    pushUndo();
                    if (fogEnabled) env.fog = Mc3::Mc3Fog{};
                    else            env.fog.reset();
                    modified_ = true; updateWindowTitle();
                }

                if (env.fog.has_value()) {
                    auto& fog = *env.fog;

                    ImGui::TextDisabled("Fog Color");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::ColorEdit3("##fogcol", fog.color.data(),
                            ImGuiColorEditFlags_NoLabel)) {
                        modified_ = true; updateWindowTitle();
                    }

                    ImGui::TextDisabled("Mode");
                    ImGui::SetNextItemWidth(-1);
                    const char* fogModes[] = { "Linear", "Exponential" };
                    int fogModeIdx = (fog.mode == Mc3::FogMode::Exponential) ? 1 : 0;
                    if (ImGui::Combo("##fogmode", &fogModeIdx, fogModes, 2)) {
                        pushUndo();
                        fog.mode = fogModeIdx == 1 ? Mc3::FogMode::Exponential : Mc3::FogMode::Linear;
                        modified_ = true; updateWindowTitle();
                    }

                    if (fog.mode == Mc3::FogMode::Linear) {
                        ImGui::TextDisabled("Start");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat("##fogstart", &fog.start, 0.5f, 0.0f, 10000.0f)) {
                            modified_ = true; updateWindowTitle();
                        }
                        ImGui::TextDisabled("End");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat("##fogend", &fog.end, 0.5f, 0.0f, 10000.0f)) {
                            modified_ = true; updateWindowTitle();
                        }
                    } else {
                        ImGui::TextDisabled("Density");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat("##fogdens", &fog.density, 0.001f, 0.0f, 1.0f, "%.4f")) {
                            modified_ = true; updateWindowTitle();
                        }
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Cameras
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Cam")) {
            if (selectedCameraIdx_ >= static_cast<int>(document_.cameras.size()))
                selectedCameraIdx_ = static_cast<int>(document_.cameras.size()) - 1;

            // Toolbar
            if (ImGui::SmallButton("+")) {
                pushUndo();
                Mc3::Mc3Camera cam;
                cam.name = "Camera " + std::to_string(document_.cameras.size() + 1);
                document_.cameras.push_back(cam);
                selectedCameraIdx_ = static_cast<int>(document_.cameras.size()) - 1;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-") && selectedCameraIdx_ >= 0) {
                pushUndo();
                const std::string& removedName = document_.cameras[selectedCameraIdx_].name;
                if (document_.defaultCamera == removedName) document_.defaultCamera.clear();
                document_.cameras.erase(document_.cameras.begin() + selectedCameraIdx_);
                selectedCameraIdx_ = std::min(selectedCameraIdx_,
                    static_cast<int>(document_.cameras.size()) - 1);
                modified_ = true; updateWindowTitle();
            }

            // Camera list
            ImGui::Separator();
            for (int i = 0; i < static_cast<int>(document_.cameras.size()); ++i) {
                const auto& cam = document_.cameras[i];
                bool isDefault = (cam.name == document_.defaultCamera);
                std::string label = (isDefault ? "* " : "  ") +
                    (cam.name.empty() ? "camera_" + std::to_string(i) : cam.name) +
                    (cam.type == Mc3::CameraType::Orthographic ? " [Ort]" : " [Per]");
                ImGui::PushID(i);
                if (ImGui::Selectable(label.c_str(), selectedCameraIdx_ == i))
                    selectedCameraIdx_ = i;
                ImGui::PopID();
            }

            // Inline editor
            if (selectedCameraIdx_ >= 0 &&
                selectedCameraIdx_ < static_cast<int>(document_.cameras.size()))
            {
                auto& cam = document_.cameras[selectedCameraIdx_];
                ImGui::Separator();
                ImGui::Spacing();

                // Name
                {
                    char buf[128];
                    std::strncpy(buf, cam.name.c_str(), sizeof(buf)-1); buf[127]='\0';
                    ImGui::TextDisabled("Name");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##cname", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo();
                        if (document_.defaultCamera == cam.name) document_.defaultCamera = buf;
                        cam.name = buf;
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Default camera toggle
                {
                    bool isDefault = (cam.name == document_.defaultCamera);
                    if (ImGui::Checkbox("Default", &isDefault)) {
                        pushUndo();
                        document_.defaultCamera = isDefault ? cam.name : "";
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Type
                {
                    const char* types[] = { "Perspective", "Orthographic" };
                    int tidx = static_cast<int>(cam.type);
                    ImGui::TextDisabled("Type");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##ctype", &tidx, types, 2)) {
                        pushUndo();
                        cam.type = static_cast<Mc3::CameraType>(tidx);
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Position
                ImGui::TextDisabled("Position");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat3("##cpos", cam.position.data(), 0.1f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }

                // Target
                ImGui::TextDisabled("Target");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat3("##ctgt", cam.target.data(), 0.1f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }

                // Rotation override (optional)
                {
                    bool hasRot = cam.rotation.has_value();
                    if (ImGui::Checkbox("Override Rotation", &hasRot)) {
                        pushUndo();
                        if (hasRot) cam.rotation = std::array<float,3>{0.0f,0.0f,0.0f};
                        else        cam.rotation.reset();
                        modified_ = true; updateWindowTitle();
                    }
                    if (cam.rotation.has_value()) {
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat3("##crot", cam.rotation->data(), 0.5f)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            modified_ = true; updateWindowTitle();
                        }
                    }
                }

                // Near / Far
                ImGui::TextDisabled("Near Plane");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##cnear", &cam.nearPlane, 0.01f, 0.001f, cam.farPlane)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Far Plane");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##cfar", &cam.farPlane, 1.0f, cam.nearPlane, 100000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }

                // Perspective-only: FOV
                if (cam.type == Mc3::CameraType::Perspective) {
                    ImGui::TextDisabled("FOV (deg)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##cfov", &cam.fov, 1.0f, 170.0f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Orthographic-only: ortho size
                if (cam.type == Mc3::CameraType::Orthographic) {
                    ImGui::TextDisabled("Ortho Size");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##cortho", &cam.orthoSize, 0.1f, 0.001f, 10000.0f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Textures
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Tex")) {
            // Validate selection
            if (!selectedTextureKey_.empty() &&
                !document_.textures.count(selectedTextureKey_))
                selectedTextureKey_.clear();

            // Toolbar: Add / Remove
            if (ImGui::SmallButton("+")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "tex_" + std::to_string(n++); }
                while (document_.textures.count(key));
                Mc3::Mc3Texture tex;
                tex.name = key;
                document_.textures[key] = tex;
                selectedTextureKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-") && !selectedTextureKey_.empty()) {
                pushUndo();
                document_.textures.erase(selectedTextureKey_);
                selectedTextureKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            // List
            ImGui::Separator();
            for (const auto& [key, tex] : document_.textures) {
                bool sel = (key == selectedTextureKey_);
                std::string label = key;
                if (!tex.uri.empty()) {
                    // Show just filename part
                    auto slash = tex.uri.find_last_of("/\\");
                    label += "  " + (slash != std::string::npos ? tex.uri.substr(slash+1) : tex.uri);
                }
                ImGui::PushID(key.c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedTextureKey_ = key;
                ImGui::PopID();
            }

            // Inline editor
            if (!selectedTextureKey_.empty() &&
                document_.textures.count(selectedTextureKey_))
            {
                auto& tex = document_.textures[selectedTextureKey_];
                ImGui::Separator();
                ImGui::Spacing();

                // ID (read-only) with copy button
                ImGui::TextDisabled("ID");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##texid"))
                    ImGui::SetClipboardText(selectedTextureKey_.c_str());
                ImGui::TextUnformatted(selectedTextureKey_.c_str());

                // Display name
                ImGui::TextDisabled("Name");
                {
                    char buf[128];
                    std::strncpy(buf, tex.name.c_str(), sizeof(buf)-1); buf[127]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##texname", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); tex.name = buf; modified_ = true; updateWindowTitle();
                    }
                }

                // URI (file path)
                ImGui::TextDisabled("URI (path)");
                {
                    char buf[512];
                    std::strncpy(buf, tex.uri.c_str(), sizeof(buf)-1); buf[511]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##texuri", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); tex.uri = buf; modified_ = true; updateWindowTitle();
                    }
                }

                // Wrap U
                {
                    const char* wraps[] = { "repeat", "clamp", "mirror" };
                    int widx = 0;
                    for (int i = 0; i < 3; ++i) if (tex.wrapU == wraps[i]) { widx = i; break; }
                    ImGui::TextDisabled("Wrap U");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##twrapu", &widx, wraps, 3)) {
                        pushUndo(); tex.wrapU = wraps[widx]; modified_ = true; updateWindowTitle();
                    }
                }

                // Wrap V
                {
                    const char* wraps[] = { "repeat", "clamp", "mirror" };
                    int widx = 0;
                    for (int i = 0; i < 3; ++i) if (tex.wrapV == wraps[i]) { widx = i; break; }
                    ImGui::TextDisabled("Wrap V");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##twrapv", &widx, wraps, 3)) {
                        pushUndo(); tex.wrapV = wraps[widx]; modified_ = true; updateWindowTitle();
                    }
                }

                // Filter
                {
                    const char* filters[] = { "linear", "nearest" };
                    int fidx = (tex.filter == "nearest") ? 1 : 0;
                    ImGui::TextDisabled("Filter");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##tfilter", &fidx, filters, 2)) {
                        pushUndo(); tex.filter = filters[fidx]; modified_ = true; updateWindowTitle();
                    }
                }

                // Color space
                {
                    const char* spaces[] = { "srgb", "linear" };
                    int sidx = (tex.colorSpace == "linear") ? 1 : 0;
                    ImGui::TextDisabled("Color Space");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##tcolorsp", &sidx, spaces, 2)) {
                        pushUndo(); tex.colorSpace = spaces[sidx]; modified_ = true; updateWindowTitle();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Definitions
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Defs")) {
            // Validate selection
            if (!selectedDefId_.empty() && !document_.definitions.count(selectedDefId_))
                selectedDefId_.clear();

            // Toolbar: Add / Remove
            if (ImGui::SmallButton("+")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "def_" + std::to_string(n++); }
                while (document_.definitions.count(key));
                auto defObj = std::make_shared<Mc3::Mc3Object>();
                defObj->id   = key;
                defObj->name = key;
                defObj->type = Mc3::ObjectType::Box;
                Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Box; p.size = {1.0f,1.0f,1.0f};
                defObj->primitive = p;
                document_.definitions[key] = defObj;
                selectedDefId_ = key;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(selectedDefId_.empty());
            if (ImGui::SmallButton("-")) {
                pushUndo();
                document_.definitions.erase(selectedDefId_);
                selectedDefId_.clear();
                modified_ = true; updateWindowTitle();
            }
            ImGui::EndDisabled();

            // List
            ImGui::Separator();
            for (const auto& [key, defObj] : document_.definitions) {
                bool sel = (key == selectedDefId_);
                std::string label = key;
                if (defObj && !defObj->name.empty() && defObj->name != key)
                    label += "  (" + defObj->name + ")";
                ImGui::PushID(key.c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedDefId_ = key;
                ImGui::PopID();
            }

            // Inline editor for selected definition
            if (!selectedDefId_.empty() && document_.definitions.count(selectedDefId_)) {
                auto& defObj = document_.definitions[selectedDefId_];
                if (defObj) {
                    ImGui::Separator();
                    ImGui::Spacing();

                    // ID (rename — updates map key + all Instance references)
                    ImGui::TextDisabled("ID");
                    {
                        char idBuf[128];
                        std::strncpy(idBuf, selectedDefId_.c_str(), sizeof(idBuf)-1);
                        idBuf[127] = '\0';
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputText("##defid", idBuf, sizeof(idBuf),
                                ImGuiInputTextFlags_EnterReturnsTrue)) {
                            std::string newKey = idBuf;
                            if (!newKey.empty() && newKey != selectedDefId_ &&
                                !document_.definitions.count(newKey)) {
                                pushUndo();
                                // Move entry to new key
                                auto node = document_.definitions.extract(selectedDefId_);
                                node.key() = newKey;
                                document_.definitions.insert(std::move(node));
                                defObj->id = newKey;
                                // Update all Instance references in the scene
                                std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> fixRefs;
                                fixRefs = [&](auto& list) {
                                    for (auto& o : list) {
                                        if (o->type == Mc3::ObjectType::Instance &&
                                            o->definition == selectedDefId_)
                                            o->definition = newKey;
                                        fixRefs(o->children);
                                    }
                                };
                                fixRefs(document_.objects);
                                selectedDefId_ = newKey;
                                modified_ = true; updateWindowTitle();
                            }
                        }
                    }

                    // Name
                    ImGui::TextDisabled("Name");
                    {
                        char nameBuf[128];
                        std::strncpy(nameBuf, defObj->name.c_str(), sizeof(nameBuf)-1);
                        nameBuf[127] = '\0';
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputText("##defname", nameBuf, sizeof(nameBuf),
                                ImGuiInputTextFlags_EnterReturnsTrue)) {
                            pushUndo(); defObj->name = nameBuf; modified_ = true; updateWindowTitle();
                        }
                    }

                    // Type
                    ImGui::TextDisabled("Type");
                    {
                        const char* typeNames[] = { "Box","Sphere","Cylinder","Cone","Plane",
                                                    "Extrude","Group","Mesh","Area" };
                        Mc3::ObjectType typeVals[] = {
                            Mc3::ObjectType::Box, Mc3::ObjectType::Sphere,
                            Mc3::ObjectType::Cylinder, Mc3::ObjectType::Cone,
                            Mc3::ObjectType::Plane, Mc3::ObjectType::Extrude,
                            Mc3::ObjectType::Group, Mc3::ObjectType::Mesh,
                            Mc3::ObjectType::Area };
                        int tidx = 0;
                        for (int i = 0; i < 9; ++i)
                            if (defObj->type == typeVals[i]) { tidx = i; break; }
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Combo("##deftype", &tidx, typeNames, 9)) {
                            pushUndo(); defObj->type = typeVals[tidx]; modified_ = true; updateWindowTitle();
                        }
                    }

                    // Transform
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::TextDisabled("Position");
                    {
                        float pos[3] = { defObj->transform.position[0], defObj->transform.position[1], defObj->transform.position[2] };
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat3("##dpos", pos, 0.1f)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            defObj->transform.position[0] = pos[0];
                            defObj->transform.position[1] = pos[1];
                            defObj->transform.position[2] = pos[2];
                            modified_ = true; updateWindowTitle();
                        }
                    }
                    ImGui::TextDisabled("Rotation");
                    {
                        float rot[3] = { defObj->transform.rotation[0], defObj->transform.rotation[1], defObj->transform.rotation[2] };
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat3("##drot", rot, 0.5f)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            defObj->transform.rotation[0] = rot[0];
                            defObj->transform.rotation[1] = rot[1];
                            defObj->transform.rotation[2] = rot[2];
                            modified_ = true; updateWindowTitle();
                        }
                    }
                    ImGui::TextDisabled("Scale");
                    {
                        float scl[3] = { defObj->transform.scale[0], defObj->transform.scale[1], defObj->transform.scale[2] };
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat3("##dscl", scl, 0.01f, 0.001f, 100.0f)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            defObj->transform.scale[0] = std::max(0.001f, scl[0]);
                            defObj->transform.scale[1] = std::max(0.001f, scl[1]);
                            defObj->transform.scale[2] = std::max(0.001f, scl[2]);
                            modified_ = true; updateWindowTitle();
                        }
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Materials
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Mat")) {
            // Validate selection
            if (!selectedMaterialKey_.empty() && !document_.materials.count(selectedMaterialKey_))
                selectedMaterialKey_.clear();

            // Toolbar: Add
            if (ImGui::SmallButton("+##matadd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "mat_" + std::to_string(n++); }
                while (document_.materials.count(key));
                Mc3::Mc3Material m;
                m.name = key;
                document_.materials[key] = m;
                selectedMaterialKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add material");
            ImGui::SameLine();
            {
                bool canRemove = !selectedMaterialKey_.empty();
                if (!canRemove) ImGui::BeginDisabled();
                if (ImGui::SmallButton("×##matrem")) {
                    pushUndo();
                    document_.materials.erase(selectedMaterialKey_);
                    selectedMaterialKey_.clear();
                    modified_ = true; updateWindowTitle();
                }
                if (!canRemove) ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Remove selected material");
            ImGui::SameLine();
            {
                bool canApply = !selectedMaterialKey_.empty() && selection_.hasSelection();
                if (!canApply) ImGui::BeginDisabled();
                if (ImGui::SmallButton("Apply##matapply")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (!lockedIds_.count(s->id))
                            s->material = selectedMaterialKey_;
                    }
                    modified_ = true; updateWindowTitle();
                    setStatusMsg("Material '" + selectedMaterialKey_ + "' applied to " +
                                 std::to_string(selection_.selection().size()) + " object(s)");
                }
                if (!canApply) ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(selection_.hasSelection()
                    ? "Apply selected material to all selected objects"
                    : "Select objects in the scene first");
            ImGui::SameLine();
            // Export (D5)
            {
                bool canExp = !selectedMaterialKey_.empty();
                if (!canExp) ImGui::BeginDisabled();
                if (ImGui::SmallButton("Exp##matexp")) {
                    matExportOpen_ = true;
                    matExportId_   = selectedMaterialKey_;
                    std::string def = selectedMaterialKey_ + ".mc3mat.xml";
                    std::strncpy(matExportBuf_, def.c_str(), sizeof(matExportBuf_)-1);
                    matExportBuf_[sizeof(matExportBuf_)-1] = '\0';
                    matExportErr_[0] = '\0';
                }
                if (!canExp) ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Export selected material to .mc3mat.xml file");
            ImGui::SameLine();
            // Import (D5)
            if (ImGui::SmallButton("Imp##matimp")) {
                matImportOpen_ = true;
                matImportBuf_[0] = '\0';
                matImportErr_[0] = '\0';
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Import material from .mc3mat.xml file");
            ImGui::Separator();

            // Search filter
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##matfilter", "Search materials…", matFilter_, sizeof(matFilter_));
            if (ImGui::IsItemHovered() && matFilter_[0] != '\0') {
                ImGui::SameLine();
            }
            if (matFilter_[0] != '\0') {
                ImGui::SameLine();
                if (ImGui::SmallButton("×##matfilterclear")) matFilter_[0] = '\0';
            }

            // Build lowercase filter string
            std::string matFiltLow(matFilter_);
            for (auto& ch : matFiltLow)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

            // Material list
            int matShown = 0;
            for (auto& [key, mat] : document_.materials) {
                // Apply filter
                if (!matFiltLow.empty()) {
                    std::string keyLow = key;
                    for (auto& ch : keyLow)
                        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    if (keyLow.find(matFiltLow) == std::string::npos) continue;
                }
                ++matShown;
                bool isSel = (key == selectedMaterialKey_);
                // Colored square preview
                ImGui::PushID(key.c_str());
                ImVec4 c(mat.baseColor[0], mat.baseColor[1], mat.baseColor[2], 1.0f);
                ImGui::ColorButton("##cb", c,
                    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                    ImVec2(14, 14));
                ImGui::SameLine(0, 4);
                if (ImGui::Selectable(key.c_str(), isSel,
                                      ImGuiSelectableFlags_SpanAllColumns,
                                      ImVec2(0, 0)))
                    selectedMaterialKey_ = key;
                ImGui::PopID();
            }
            if (document_.materials.empty())
                ImGui::TextDisabled("No materials — press + to add");
            else if (matShown == 0)
                ImGui::TextDisabled("No match for \"%s\"", matFilter_);

            // Editor for selected material
            if (!selectedMaterialKey_.empty() && document_.materials.count(selectedMaterialKey_)) {
                auto& mat = document_.materials[selectedMaterialKey_];
                ImGui::Separator();

                // Rename key
                {
                    char kbuf[128];
                    std::strncpy(kbuf, selectedMaterialKey_.c_str(), sizeof(kbuf) - 1);
                    kbuf[sizeof(kbuf)-1] = '\0';
                    ImGui::TextDisabled("ID");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##matkey", kbuf, sizeof(kbuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                        std::string newKey(kbuf);
                        if (!newKey.empty() && !document_.materials.count(newKey)) {
                            pushUndo();
                            Mc3::Mc3Material tmp = mat;
                            document_.materials.erase(selectedMaterialKey_);
                            // Update any objects referencing the old key
                            std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> fixRefs;
                            fixRefs = [&](auto& list) {
                                for (auto& o : list) {
                                    if (o->material == selectedMaterialKey_) o->material = newKey;
                                    fixRefs(o->children);
                                }
                            };
                            fixRefs(document_.objects);
                            document_.materials[newKey] = std::move(tmp);
                            selectedMaterialKey_ = newKey;
                            modified_ = true; updateWindowTitle();
                        }
                    }
                }

                // Base color
                ImGui::TextDisabled("Base Color");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit4("##matbc", mat.baseColor.data(),
                    ImGuiColorEditFlags_Float)) {
                    modified_ = true;
                }

                // Roughness
                ImGui::TextDisabled("Roughness");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##matrgh", &mat.roughness, 0.0f, 1.0f, "%.2f")) {
                    modified_ = true;
                }

                // Metallic
                ImGui::TextDisabled("Metallic");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##matmet", &mat.metallic, 0.0f, 1.0f, "%.2f")) {
                    modified_ = true;
                }

                // Emissive
                ImGui::TextDisabled("Emissive");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##matemi", mat.emissiveColor.data(),
                    ImGuiColorEditFlags_Float)) {
                    modified_ = true;
                }

                // Alpha mode
                ImGui::TextDisabled("Alpha Mode");
                const char* alphaModes[] = { "opaque", "mask", "blend" };
                int alphaIdx = 0;
                if (mat.alphaMode == "mask")  alphaIdx = 1;
                if (mat.alphaMode == "blend") alphaIdx = 2;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##matam", &alphaIdx, alphaModes, 3)) {
                    mat.alphaMode = alphaModes[alphaIdx];
                    modified_ = true;
                }
                if (alphaIdx == 1) {
                    ImGui::TextDisabled("Alpha Cutoff");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##matac", &mat.alphaCutoff, 0.0f, 1.0f, "%.2f"))
                        modified_ = true;
                }

                // Double-sided
                if (ImGui::Checkbox("Double-sided##matds", &mat.doubleSided))
                    modified_ = true;
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleColor();

    // -----------------------------------------------------------------------

}


} // namespace MeshCraft
