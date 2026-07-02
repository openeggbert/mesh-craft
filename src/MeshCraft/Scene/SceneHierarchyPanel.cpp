#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace MeshCraft::Scene {

SceneHierarchyPanel::SceneHierarchyPanel(Mc3::Mc3Document& document)
    : document_(document)
{}

void SceneHierarchyPanel::draw(Editor::SelectionManager& selection,
                                std::set<std::string>& lockedIds,
                                const HierarchyCallbacks& cb)
{
    // --- Drag-and-drop helpers ---
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
            return;
        cb.pushUndo();
        detachObj(document_.objects, dragId);
        if (newParent) newParent->children.push_back(dragged);
        else           document_.objects.push_back(dragged);
        cb.markModified();
    };

    // --- Search filter + expand/collapse all ---
    float btnW = 22.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btnW * 2 - ImGui::GetStyle().ItemSpacing.x * 2);
    ImGui::InputTextWithHint("##hfilter", "Search...", searchBuf_, sizeof(searchBuf_));
    if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        searchBuf_[0] = '\0';
    ImGui::SameLine();
    bool expandAll   = ImGui::SmallButton("+##ea");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Expand All");
    ImGui::SameLine();
    bool collapseAll = ImGui::SmallButton("-##ca");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Collapse All");

    // --- Type filter bar ---
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
            bool active = (typeFilter_ == b.id);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.20f, 1.f));
            if (ImGui::SmallButton(b.label)) typeFilter_ = b.id;
            if (active) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", b.tip);
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }

    // --- Layer / Tag / Material filters + AND/OR toggle ---
    {
        std::set<std::string> layerNames, tagNames;
        std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> collectMeta;
        collectMeta = [&](const auto& list) {
            for (const auto& o : list) {
                if (!o->layer.empty()) layerNames.insert(o->layer);
                for (const auto& t : o->tags) if (!t.empty()) tagNames.insert(t);
                collectMeta(o->children);
            }
        };
        collectMeta(document_.objects);

        {
            bool isOr = filterOr_;
            if (isOr) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.20f, 0.20f, 1.f));
            else      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.35f, 0.45f, 1.f));
            if (ImGui::SmallButton(isOr ? "OR##andor" : "AND##andor")) filterOr_ = !filterOr_;
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(isOr
                    ? "OR mode: show object if ANY active filter matches"
                    : "AND mode: show object only if ALL active filters match");
            ImGui::SameLine();
            ImGui::TextDisabled("mode");
        }

        if (!layerNames.empty()) {
            ImGui::SameLine();
            ImGui::Text("Lay:");
            ImGui::SameLine();
            float comboW = std::min(90.f, ImGui::GetContentRegionAvail().x - 4.f);
            ImGui::SetNextItemWidth(comboW);
            if (ImGui::BeginCombo("##layfilter",
                                  layerFilter_.empty() ? "*" : layerFilter_.c_str())) {
                if (ImGui::Selectable("* (any)", layerFilter_.empty())) layerFilter_.clear();
                for (const auto& ln : layerNames) {
                    bool sel = (ln == layerFilter_);
                    if (ImGui::Selectable(ln.c_str(), sel)) layerFilter_ = ln;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (!layerFilter_.empty()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("×##layclr")) layerFilter_.clear();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear layer filter");
            }
        }

        if (!tagNames.empty()) {
            ImGui::SameLine();
            ImGui::Text("Tag:");
            ImGui::SameLine();
            float comboW = std::min(90.f, ImGui::GetContentRegionAvail().x - 4.f);
            ImGui::SetNextItemWidth(comboW);
            if (ImGui::BeginCombo("##tagfilter",
                                  tagFilter_.empty() ? "*" : tagFilter_.c_str())) {
                if (ImGui::Selectable("* (any)", tagFilter_.empty())) tagFilter_.clear();
                for (const auto& tn : tagNames) {
                    bool sel = (tn == tagFilter_);
                    if (ImGui::Selectable(tn.c_str(), sel)) tagFilter_ = tn;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (!tagFilter_.empty()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("×##tagclr")) tagFilter_.clear();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear tag filter");
            }
        }

        if (!document_.materials.empty()) {
            ImGui::SameLine();
            ImGui::Text("Mat:");
            ImGui::SameLine();
            float comboW = std::min(90.f, ImGui::GetContentRegionAvail().x - 4.f);
            ImGui::SetNextItemWidth(comboW);
            if (ImGui::BeginCombo("##matfilter",
                                  matFilter_.empty() ? "*" : matFilter_.c_str())) {
                if (ImGui::Selectable("* (any)", matFilter_.empty())) matFilter_.clear();
                for (const auto& [key, _] : document_.materials) {
                    bool sel = (key == matFilter_);
                    if (ImGui::Selectable(key.c_str(), sel)) matFilter_ = key;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (!matFilter_.empty()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("×##matclr")) matFilter_.clear();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear material filter");
            }
        }
    }
    ImGui::Separator();

    // Build lowercase filter string
    std::string filterLower = searchBuf_;
    for (auto& ch : filterLower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    bool filtering     = !filterLower.empty();
    bool typeFiltering = (typeFilter_ != 0);
    bool layFiltering  = !layerFilter_.empty();
    bool tagFiltering  = !tagFilter_.empty();
    bool matFiltering  = !matFilter_.empty();
    bool anyFiltering  = filtering || typeFiltering || layFiltering || tagFiltering || matFiltering;

    auto matchesType = [&](const Mc3::Mc3Object& o) -> bool {
        using OT = Mc3::ObjectType;
        switch (typeFilter_) {
        case 1:
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

    std::function<bool(const Mc3::Mc3Object&)> matchesFilter;
    matchesFilter = [&](const Mc3::Mc3Object& o) -> bool {
        if (!anyFiltering) return true;
        std::string nl = o.name.empty() ? o.id : o.name;
        for (auto& ch : nl) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        bool textOk = !filtering     || nl.find(filterLower) != std::string::npos;
        bool typeOk = !typeFiltering || matchesType(o);
        bool layOk  = !layFiltering  || (o.layer == layerFilter_);
        bool tagOk  = !tagFiltering  || std::any_of(o.tags.begin(), o.tags.end(),
                                            [&](const std::string& t){ return t == tagFilter_; });
        bool matOk  = !matFiltering  || (o.material == matFilter_);
        bool selfMatch = filterOr_
            ? (   (filtering     && textOk)
               || (typeFiltering && typeOk)
               || (layFiltering  && layOk)
               || (tagFiltering  && tagOk)
               || (matFiltering  && matOk))
            : (textOk && typeOk && layOk && tagOk && matOk);
        if (selfMatch) return true;
        for (const auto& c : o.children) if (matchesFilter(*c)) return true;
        return false;
    };

    // Rebuild flat order for shift-click range selection
    flatOrder_.clear();
    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> buildFlat;
    buildFlat = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
        for (const auto& o : list) {
            flatOrder_.push_back(o);
            buildFlat(o->children);
        }
    };
    buildFlat(document_.objects);

    // --- Hierarchy draw ---
    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> drawHierarchy;
    drawHierarchy = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
        for (const auto& obj : list) {
            // anyFiltering, not filtering (text-only): a type/layer/tag/
            // material filter with no search text typed must still narrow
            // the list — matchesFilter() already accounts for all of them
            // via anyFiltering, but gating the call on `filtering` alone
            // made the other filters silently do nothing (STAB-0306).
            if (anyFiltering && !matchesFilter(*obj)) continue;
            ImGui::PushID(obj.get());
            bool sel = selection.isSelected(obj.get());
            bool hasChildren = !obj->children.empty();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;
            if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (anyFiltering && hasChildren) flags |= ImGuiTreeNodeFlags_DefaultOpen;
            if (sel)          flags |= ImGuiTreeNodeFlags_Selected;
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
            bool isLocked = lockedIds.count(obj->id) > 0;
            std::string displayLabel = std::string(typePrefix) + (obj->name.empty() ? obj->id : obj->name);

            if (!renamingId_.empty() && obj->id == renamingId_) {
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
                        cb.pushUndo();
                        obj->name = renameBuf_;
                        cb.markModified();
                    }
                    renamingId_.clear();
                }
            } else {
                if (!scrollToId_.empty() && obj->id == scrollToId_) {
                    ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                    ImGui::SetScrollHereY(0.5f);
                    scrollToId_.clear();
                }

                ImGui::PushStyleColor(ImGuiCol_Text, nodeColor);
                ImGui::SetNextItemAllowOverlap();
                bool nodeOpen = ImGui::TreeNodeEx(displayLabel.c_str(), flags);
                ImGui::PopStyleColor();

                if (isLocked) {
                    ImVec2 rMin = ImGui::GetItemRectMin();
                    ImVec2 rMax = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        rMin, ImVec2(rMin.x + 4.0f, rMax.y),
                        IM_COL32(230, 140, 20, 200));
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        rMin, rMax, IM_COL32(230, 140, 20, 28));
                }

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("MC3_OBJ", obj->id.c_str(), obj->id.size() + 1);
                    ImGui::Text("%s", displayLabel.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("MC3_OBJ"))
                        doReparent(std::string(static_cast<const char*>(pl->Data)), obj);
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    const bool ctrlDown  = ImGui::GetIO().KeyCtrl;
                    const bool shiftDown = ImGui::GetIO().KeyShift;
                    if (shiftDown && !anchorId_.empty()) {
                        auto findIdx = [&](const std::string& id) -> int {
                            for (int i = 0; i < static_cast<int>(flatOrder_.size()); ++i)
                                if (flatOrder_[static_cast<size_t>(i)]->id == id) return i;
                            return -1;
                        };
                        int a = findIdx(anchorId_);
                        int b = findIdx(obj->id);
                        if (a >= 0 && b >= 0) {
                            if (!ctrlDown) selection.clear();
                            if (a > b) std::swap(a, b);
                            for (int i = a; i <= b; ++i)
                                selection.select(flatOrder_[static_cast<size_t>(i)]);
                        }
                    } else if (ctrlDown) {
                        if (selection.isSelected(obj.get())) selection.deselect(obj);
                        else                                   selection.select(obj);
                        anchorId_ = obj->id;
                    } else {
                        selection.clear();
                        selection.select(obj);
                        anchorId_ = obj->id;
                    }
                }
                if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) {
                    renamingId_ = obj->id;
                    std::strncpy(renameBuf_, obj->name.c_str(), sizeof(renameBuf_) - 1);
                    renameBuf_[sizeof(renameBuf_) - 1] = '\0';
                    renameNeedsFocus_ = true;
                }
                if (ImGui::BeginPopupContextItem("##objctx")) {
                    if (ImGui::MenuItem("Select Parent\tP")) cb.selectParent();
                    if (ImGui::MenuItem("Select Children", nullptr, false, !obj->children.empty()))
                        cb.selectChildren();
                    ImGui::Separator();
                    if (ImGui::MenuItem("Rename")) {
                        renamingId_ = obj->id;
                        std::strncpy(renameBuf_, obj->name.c_str(), sizeof(renameBuf_) - 1);
                        renameBuf_[sizeof(renameBuf_) - 1] = '\0';
                        renameNeedsFocus_ = true;
                    }
                    if (ImGui::MenuItem("Batch Rename...\tCtrl+Shift+R")) cb.openBatchRename();
                    if (ImGui::MenuItem("Duplicate")) cb.duplicateSel();
                    if (ImGui::MenuItem("Delete", nullptr, false, !isLocked)) cb.deleteSel();
                    ImGui::Separator();
                    if (ImGui::MenuItem("Group Selection")) {
                        cb.pushUndo();
                        int gn = 1;
                        std::string gid;
                        do { gid = "group_" + std::to_string(gn++); }
                        while (findObj(document_.objects, gid) != nullptr);
                        auto grp = std::make_shared<Mc3::Mc3Object>();
                        grp->id   = gid;
                        grp->name = gid;
                        grp->type = Mc3::ObjectType::Group;
                        grp->transform.scale = {1.0f, 1.0f, 1.0f};
                        auto selCopy = selection.selection();
                        for (const auto& s : selCopy) {
                            detachObj(document_.objects, s->id);
                            grp->children.push_back(s);
                        }
                        document_.objects.push_back(grp);
                        selection.clear();
                        selection.select(grp);
                        cb.markModified();
                    }
                    if (ImGui::MenuItem("Set as Root"))
                        doReparent(obj->id, nullptr);
                    ImGui::Separator();
                    if (ImGui::MenuItem(obj->visible ? "Hide" : "Show")) {
                        cb.pushUndo();
                        obj->visible = !obj->visible;
                        cb.markModified();
                    }
                    if (ImGui::MenuItem(isLocked ? "Unlock\tCtrl+L" : "Lock\tCtrl+L")) {
                        if (isLocked) lockedIds.erase(obj->id);
                        else          lockedIds.insert(obj->id);
                    }
                    ImGui::EndPopup();
                }

                // Lock + visibility buttons — right-aligned
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1,1));
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
                        if (isLocked) lockedIds.erase(obj->id);
                        else          lockedIds.insert(obj->id);
                    }
                    ImGui::PopStyleColor(3);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(isLocked ? "Locked — click to unlock (Ctrl+L)" : "Click to lock (Ctrl+L)");

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
                        cb.pushUndo();
                        obj->visible = !obj->visible;
                        cb.markModified();
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

    // Root-level drop zone
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
}

} // namespace MeshCraft::Scene
