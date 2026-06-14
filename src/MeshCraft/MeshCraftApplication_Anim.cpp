#include "MeshCraft/MeshCraftApplication.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <utility>

namespace MeshCraft {

using namespace Microsoft::Xna::Framework;

void MeshCraftApplication::evaluateAndPushAnimOverrides() {
    // If the current action no longer exists in the document, clear it
    if (!currentActionName_.empty() && !document_.actions.count(currentActionName_)) {
        currentActionName_.clear();
        animPlaying_ = false;
    }
    if (currentActionName_.empty()) {
        sceneRenderer_->setAnimOverrides({});
        return;
    }

    const auto& action = document_.actions.at(currentActionName_);
    std::unordered_map<std::string, Renderer::AnimOverride> overrides;

    // First pass: for each target object that has channels, initialize the
    // override from the object's current document-state (transform + material).
    for (const auto& ch : action.channels) {
        if (ch.targetObject.empty() || overrides.count(ch.targetObject)) continue;
        Mc3::Mc3Object* obj = flatFindByName(ch.targetObject);
        if (!obj) continue;
        auto& ov = overrides[ch.targetObject];
        ov.position = obj->transform.position;
        ov.rotation = obj->transform.rotation;
        ov.scale    = obj->transform.scale;
        ov.visible  = obj->visible;
        if (!obj->material.empty()) {
            auto matIt = document_.materials.find(obj->material);
            if (matIt != document_.materials.end()) {
                const auto& m = matIt->second;
                ov.baseColor = m.baseColor;
                ov.roughness = m.roughness;
                ov.metallic  = m.metallic;
                ov.emissive  = std::array<float,3>{m.emissiveColor[0], m.emissiveColor[1], m.emissiveColor[2]};
            }
        }
        ov.deformScale = obj->deform
            ? obj->deform->scale
            : std::array<float,3>{1.0f, 1.0f, 1.0f};
    }

    // Second pass: apply evaluated channel values at the current time
    using AP = Mc3::AnimatedProperty;
    for (const auto& ch : action.channels) {
        auto it = overrides.find(ch.targetObject);
        if (it == overrides.end()) continue;
        float v = Mc3::evaluateChannel(ch, animTime_);
        auto& ov = it->second;
        switch (ch.property) {
        case AP::PositionX: (*ov.position)[0] = v; break;
        case AP::PositionY: (*ov.position)[1] = v; break;
        case AP::PositionZ: (*ov.position)[2] = v; break;
        case AP::RotationX: (*ov.rotation)[0] = v; break;
        case AP::RotationY: (*ov.rotation)[1] = v; break;
        case AP::RotationZ: (*ov.rotation)[2] = v; break;
        case AP::ScaleX:    (*ov.scale)[0]    = v; break;
        case AP::ScaleY:    (*ov.scale)[1]    = v; break;
        case AP::ScaleZ:    (*ov.scale)[2]    = v; break;
        case AP::Visible:   ov.visible        = (v >= 0.5f); break;
        case AP::MaterialBaseColorR: if (ov.baseColor) (*ov.baseColor)[0] = v; break;
        case AP::MaterialBaseColorG: if (ov.baseColor) (*ov.baseColor)[1] = v; break;
        case AP::MaterialBaseColorB: if (ov.baseColor) (*ov.baseColor)[2] = v; break;
        case AP::MaterialBaseColorA: if (ov.baseColor) (*ov.baseColor)[3] = v; break;
        case AP::MaterialRoughness:  ov.roughness = v; break;
        case AP::MaterialMetallic:   ov.metallic  = v; break;
        case AP::MaterialEmissiveR:  if (ov.emissive) (*ov.emissive)[0] = v; break;
        case AP::MaterialEmissiveG:  if (ov.emissive) (*ov.emissive)[1] = v; break;
        case AP::MaterialEmissiveB:  if (ov.emissive) (*ov.emissive)[2] = v; break;
        case AP::DeformX: if (ov.deformScale) (*ov.deformScale)[0] = v; break;
        case AP::DeformY: if (ov.deformScale) (*ov.deformScale)[1] = v; break;
        case AP::DeformZ: if (ov.deformScale) (*ov.deformScale)[2] = v; break;
        default: break;
        }
    }

    sceneRenderer_->setAnimOverrides(std::move(overrides));
}

void MeshCraftApplication::insertAnimKeyframes(
    Mc3::Mc3Object& obj,
    std::initializer_list<Mc3::AnimatedProperty> props)
{
    if (currentActionName_.empty()) return;
    auto actionIt = document_.actions.find(currentActionName_);
    if (actionIt == document_.actions.end()) return;

    pushUndo();
    auto& action = document_.actions[currentActionName_]; // re-find after pushUndo (safe: same map)

    for (auto prop : props) {
        // Find or create channel
        int ci = -1;
        for (int i = 0; i < (int)action.channels.size(); ++i) {
            if (action.channels[i].targetObject == obj.name &&
                action.channels[i].property == prop) { ci = i; break; }
        }
        if (ci < 0) {
            Mc3::Mc3Channel ch;
            ch.targetObject = obj.name;
            ch.property     = prop;
            action.channels.push_back(std::move(ch));
            ci = static_cast<int>(action.channels.size()) - 1;
        }

        // Evaluate current value from the object
        float value = 0.0f;
        switch (prop) {
        case Mc3::AnimatedProperty::PositionX: value = obj.transform.position[0]; break;
        case Mc3::AnimatedProperty::PositionY: value = obj.transform.position[1]; break;
        case Mc3::AnimatedProperty::PositionZ: value = obj.transform.position[2]; break;
        case Mc3::AnimatedProperty::RotationX: value = obj.transform.rotation[0]; break;
        case Mc3::AnimatedProperty::RotationY: value = obj.transform.rotation[1]; break;
        case Mc3::AnimatedProperty::RotationZ: value = obj.transform.rotation[2]; break;
        case Mc3::AnimatedProperty::ScaleX:    value = obj.transform.scale[0];    break;
        case Mc3::AnimatedProperty::ScaleY:    value = obj.transform.scale[1];    break;
        case Mc3::AnimatedProperty::ScaleZ:    value = obj.transform.scale[2];    break;
        case Mc3::AnimatedProperty::Visible:   value = obj.visible ? 1.0f : 0.0f; break;
        default:
            value = Mc3::evaluateChannel(action.channels[ci], animTime_); break;
        }

        auto& ch = action.channels[ci];
        // Replace existing keyframe at this time, or insert a new one
        bool replaced = false;
        for (auto& kf : ch.keyframes) {
            if (std::abs(kf.time - animTime_) < 0.001f) { kf.value = value; replaced = true; break; }
        }
        if (!replaced) {
            Mc3::Mc3Keyframe kf;
            kf.time = animTime_; kf.value = value;
            ch.keyframes.push_back(kf);
            std::sort(ch.keyframes.begin(), ch.keyframes.end(),
                [](const Mc3::Mc3Keyframe& a, const Mc3::Mc3Keyframe& b){ return a.time < b.time; });
        }
    }
    modified_ = true;
    evaluateAndPushAnimOverrides();
}

void MeshCraftApplication::drawTimelinePanel(int screenW, int screenH) {
    int panelTop = screenH - kStatusH - kTimelineH;
    ImGui::SetNextWindowPos(ImVec2(0.0f, static_cast<float>(panelTop)));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(screenW), static_cast<float>(kTimelineH)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.15f, 1.0f));
    ImGui::Begin("##timeline", nullptr,
        ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove        | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);

    // ----- Control bar (row 1) -----
    {
        ImGui::Text("Action:");
        ImGui::SameLine();
        const char* preview = currentActionName_.empty() ? "(none)" : currentActionName_.c_str();
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::BeginCombo("##asel", preview)) {
            if (ImGui::Selectable("(none)", currentActionName_.empty())) {
                currentActionName_.clear(); animPlaying_ = false;
                sceneRenderer_->setAnimOverrides({});
            }
            for (auto& [nm, act] : document_.actions) {
                bool isSel = (nm == currentActionName_);
                if (ImGui::Selectable(nm.c_str(), isSel)) {
                    currentActionName_ = nm; animTime_ = 0.0f; animPlaying_ = false;
                    evaluateAndPushAnimOverrides();
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("+##naact")) {
            int n = 1; std::string nm;
            do { nm = "Action" + std::to_string(n++); } while (document_.actions.count(nm));
            Mc3::Mc3Action act; act.name = nm; act.duration = 2.0f;
            pushUndo(); document_.actions[nm] = std::move(act);
            currentActionName_ = nm; animTime_ = 0.0f; animPlaying_ = false;
            modified_ = true;
        }
        ImGui::SameLine();
        bool hasAct = !currentActionName_.empty() && document_.actions.count(currentActionName_);
        if (!hasAct) ImGui::BeginDisabled();
        if (ImGui::SmallButton("Del##daact") && hasAct) {
            pushUndo(); document_.actions.erase(currentActionName_);
            currentActionName_.clear(); animPlaying_ = false;
            sceneRenderer_->setAnimOverrides({}); modified_ = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Dup##daact") && hasAct) {
            pushUndo();
            std::string base = currentActionName_ + "_copy";
            std::string nm = base;
            int n = 2;
            while (document_.actions.count(nm)) nm = base + std::to_string(n++);
            document_.actions[nm] = document_.actions[currentActionName_];
            document_.actions[nm].name = nm;
            currentActionName_ = nm;
            modified_ = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Ren##raact") && hasAct) {
            renameActionOpen_ = true;
            std::strncpy(renameActionBuf_, currentActionName_.c_str(),
                         sizeof(renameActionBuf_) - 1);
            renameActionBuf_[sizeof(renameActionBuf_)-1] = '\0';
        }
        if (!hasAct) ImGui::EndDisabled();

        if (hasAct) {
            auto& act = document_.actions[currentActionName_];
            ImGui::SameLine(); ImGui::Text("|"); ImGui::SameLine();
            ImGui::Text("Dur:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(55.0f);
            float dur = act.duration;
            if (ImGui::DragFloat("##dur", &dur, 0.01f, 0.01f, 3600.0f, "%.2f")) {
                act.duration = std::max(0.01f, dur);
                animTime_    = std::min(animTime_, act.duration);
                modified_    = true;
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Loop##lp", &act.loop)) modified_ = true;
            ImGui::SameLine(); ImGui::Text("|"); ImGui::SameLine();
            if (ImGui::SmallButton("|<##rew"))  { animTime_ = 0.0f; evaluateAndPushAnimOverrides(); }
            ImGui::SameLine();
            if (animPlaying_) { if (ImGui::SmallButton("||##pp")) animPlaying_ = false; }
            else              { if (ImGui::SmallButton("|>##pp")) animPlaying_ = true;  }
            ImGui::SameLine();
            if (ImGui::SmallButton("[]##stp")) { animPlaying_ = false; animTime_ = 0.0f; evaluateAndPushAnimOverrides(); }
            ImGui::SameLine(); ImGui::Text("|"); ImGui::SameLine();
            ImGui::Text("T:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            if (ImGui::DragFloat("##at", &animTime_, 0.001f, 0.0f, act.duration, "%.3f")) {
                animTime_ = std::clamp(animTime_, 0.0f, act.duration);
                evaluateAndPushAnimOverrides();
            }
        }
    }

    // ----- Channel list + timeline track (row 2+) -----
    const float rowH = 18.0f;
    const float leftW = 240.0f;
    float availH = ImGui::GetContentRegionAvail().y;

    bool hasAct = !currentActionName_.empty() && document_.actions.count(currentActionName_);

    // Left column — channel labels
    ImGui::BeginChild("##tlchan", ImVec2(leftW, availH), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (hasAct) {
        auto& act = document_.actions[currentActionName_];
        int toDelete = -1;
        for (int ci = 0; ci < (int)act.channels.size(); ++ci) {
            ImGui::PushID(ci);
            auto& ch = act.channels[ci];
            std::string lbl = ch.targetObject + " / " +
                              Mc3::animatedPropertyName(ch.property);
            float textY = ci * rowH + (rowH - ImGui::GetTextLineHeight()) * 0.5f;
            ImGui::SetCursorPosY(textY);
            ImGui::TextUnformatted(lbl.c_str());
            ImGui::SameLine(leftW - 36.0f);
            ImGui::SetCursorPosY(ci * rowH + 2.0f);
            if (ImGui::SmallButton("X##dc")) toDelete = ci;
            ImGui::PopID();
        }
        if (toDelete >= 0) {
            pushUndo(); act.channels.erase(act.channels.begin() + toDelete);
            if (tlSelChan_ == toDelete) { tlSelChan_ = -1; tlSelKf_ = -1; }
            tlMultiSel_.clear();
            modified_ = true; evaluateAndPushAnimOverrides();
        }

        // Add channel button
        float btnY = static_cast<float>(act.channels.size()) * rowH + 2.0f;
        ImGui::SetCursorPosY(btnY);
        if (ImGui::SmallButton("+ Channel")) {
            addChannelOpen_ = true;
            addChannelObjBuf_[0] = '\0';
            // Pre-fill from first selected object
            if (selection_.hasSelection())
                std::strncpy(addChannelObjBuf_,
                             selection_.selection().front()->name.c_str(),
                             sizeof(addChannelObjBuf_) - 1);
        }
    }
    ImGui::EndChild();

    // Add Channel popup (modal, shown on top of timeline)
    if (addChannelOpen_) {
        ImGui::OpenPopup("Add Channel##acdlg");
        addChannelOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Add Channel##acdlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        // Property table
        static const Mc3::AnimatedProperty kAllProps[] = {
            Mc3::AnimatedProperty::PositionX, Mc3::AnimatedProperty::PositionY, Mc3::AnimatedProperty::PositionZ,
            Mc3::AnimatedProperty::RotationX, Mc3::AnimatedProperty::RotationY, Mc3::AnimatedProperty::RotationZ,
            Mc3::AnimatedProperty::ScaleX,    Mc3::AnimatedProperty::ScaleY,    Mc3::AnimatedProperty::ScaleZ,
            Mc3::AnimatedProperty::Visible,
            Mc3::AnimatedProperty::DeformX,   Mc3::AnimatedProperty::DeformY,   Mc3::AnimatedProperty::DeformZ,
            Mc3::AnimatedProperty::MaterialBaseColorR, Mc3::AnimatedProperty::MaterialBaseColorG,
            Mc3::AnimatedProperty::MaterialBaseColorB, Mc3::AnimatedProperty::MaterialBaseColorA,
            Mc3::AnimatedProperty::MaterialRoughness,  Mc3::AnimatedProperty::MaterialMetallic,
            Mc3::AnimatedProperty::MaterialEmissiveR,  Mc3::AnimatedProperty::MaterialEmissiveG,
            Mc3::AnimatedProperty::MaterialEmissiveB,
        };
        constexpr int kPropCount = static_cast<int>(std::size(kAllProps));

        ImGui::TextDisabled("Object");
        ImGui::SetNextItemWidth(260);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##acobj", "Object name…", addChannelObjBuf_,
                                 sizeof(addChannelObjBuf_));
        // Autocomplete list
        {
            std::vector<std::string> names;
            std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> collect;
            collect = [&](const auto& list) {
                for (const auto& o : list) {
                    if (!o->name.empty()) names.push_back(o->name);
                    collect(o->children);
                }
            };
            collect(document_.objects);
            std::string filter(addChannelObjBuf_);
            for (const auto& n : names) {
                if (filter.empty() || n.find(filter) != std::string::npos) {
                    if (ImGui::SmallButton(n.c_str())) {
                        std::strncpy(addChannelObjBuf_, n.c_str(),
                                     sizeof(addChannelObjBuf_) - 1);
                        addChannelObjBuf_[sizeof(addChannelObjBuf_)-1] = '\0';
                    }
                    ImGui::SameLine();
                }
            }
            ImGui::NewLine();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Property");
        ImGui::SetNextItemWidth(260);
        // Build names array for combo
        static const char* kPropNames[kPropCount] = {};
        for (int i = 0; i < kPropCount; ++i)
            kPropNames[i] = Mc3::animatedPropertyName(kAllProps[i]);
        ImGui::Combo("##acprop", &addChannelPropIdx_, kPropNames, kPropCount);

        // Warn if channel already exists
        bool hasAct2 = !currentActionName_.empty() && document_.actions.count(currentActionName_);
        bool duplicate = false;
        if (hasAct2 && addChannelObjBuf_[0]) {
            Mc3::AnimatedProperty chosenProp = kAllProps[addChannelPropIdx_];
            for (const auto& ch : document_.actions[currentActionName_].channels) {
                if (ch.targetObject == addChannelObjBuf_ && ch.property == chosenProp) {
                    duplicate = true; break;
                }
            }
        }
        if (duplicate)
            ImGui::TextColored(ImVec4(1,0.5f,0.2f,1), "Channel already exists for this object+property.");

        ImGui::Spacing();
        bool canAdd = addChannelObjBuf_[0] && hasAct2 && !duplicate;
        if (!canAdd) ImGui::BeginDisabled();
        if (ImGui::Button("Add", ImVec2(90, 0))) {
            auto& act2 = document_.actions[currentActionName_];
            Mc3::AnimatedProperty prop = kAllProps[addChannelPropIdx_];
            // Read initial value from object if present
            float initVal = 0.0f;
            auto* obj = flatFindByName(addChannelObjBuf_);
            if (obj) {
                using AP = Mc3::AnimatedProperty;
                switch (prop) {
                    case AP::PositionX: initVal = obj->transform.position[0]; break;
                    case AP::PositionY: initVal = obj->transform.position[1]; break;
                    case AP::PositionZ: initVal = obj->transform.position[2]; break;
                    case AP::RotationX: initVal = obj->transform.rotation[0]; break;
                    case AP::RotationY: initVal = obj->transform.rotation[1]; break;
                    case AP::RotationZ: initVal = obj->transform.rotation[2]; break;
                    case AP::ScaleX:    initVal = obj->transform.scale[0];    break;
                    case AP::ScaleY:    initVal = obj->transform.scale[1];    break;
                    case AP::ScaleZ:    initVal = obj->transform.scale[2];    break;
                    case AP::Visible:   initVal = obj->visible ? 1.0f : 0.0f; break;
                    default: break;
                }
            }
            Mc3::Mc3Channel ch;
            ch.targetObject = addChannelObjBuf_;
            ch.property     = prop;
            Mc3::Mc3Keyframe kf;
            kf.time  = animTime_;
            kf.value = initVal;
            kf.interpolation = Mc3::Interpolation::Linear;
            ch.keyframes.push_back(kf);
            pushUndo();
            act2.channels.push_back(std::move(ch));
            tlSelChan_ = static_cast<int>(act2.channels.size()) - 1;
            tlSelKf_   = 0;
            modified_ = true;
            evaluateAndPushAnimOverrides();
            ImGui::CloseCurrentPopup();
        }
        if (!canAdd) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Rename Action popup
    if (renameActionOpen_) {
        ImGui::OpenPopup("Rename Action##radlg");
        renameActionOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Rename Action##radlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("New name:");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##ranm", renameActionBuf_, sizeof(renameActionBuf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        bool nameTaken = document_.actions.count(renameActionBuf_) &&
                         std::string(renameActionBuf_) != currentActionName_;
        bool empty = (renameActionBuf_[0] == '\0');
        if (nameTaken)
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Name already in use.");
        bool canRen = !empty && !nameTaken;
        if (!canRen) ImGui::BeginDisabled();
        if ((enter || ImGui::Button("Rename", ImVec2(90, 0))) && canRen) {
            pushUndo();
            std::string newName(renameActionBuf_);
            auto node = document_.actions.extract(currentActionName_);
            node.key() = newName;
            node.mapped().name = newName;
            document_.actions.insert(std::move(node));
            currentActionName_ = newName;
            modified_ = true;
            ImGui::CloseCurrentPopup();
        }
        if (!canRen) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Right column — timeline track with keyframe dots
    ImGui::SameLine();
    ImVec2 trackTL = ImGui::GetCursorScreenPos();
    float  trackW  = ImGui::GetContentRegionAvail().x;

    ImGui::BeginChild("##tltrack", ImVec2(trackW, availH), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    float dur = (hasAct) ? document_.actions[currentActionName_].duration : 2.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Ruler background
    dl->AddRectFilled(trackTL,
                      ImVec2(trackTL.x + trackW, trackTL.y + rowH),
                      IM_COL32(22, 25, 40, 255));

    // Time tick marks
    float tickStep = 0.25f;
    if      (dur > 30.0f) tickStep = 10.0f;
    else if (dur > 10.0f) tickStep = 5.0f;
    else if (dur > 5.0f)  tickStep = 1.0f;
    else if (dur > 2.0f)  tickStep = 0.5f;

    for (float t = 0.0f; t < dur + tickStep * 0.01f; t += tickStep) {
        t = std::min(t, dur);
        float x = trackTL.x + (t / dur) * trackW;
        dl->AddLine(ImVec2(x, trackTL.y), ImVec2(x, trackTL.y + rowH),
                    IM_COL32(60, 70, 100, 200));
        char buf[16]; snprintf(buf, sizeof(buf), "%.2f", t);
        dl->AddText(ImVec2(x + 2, trackTL.y + 2), IM_COL32(130, 145, 175, 230), buf);
    }

    // Current-time indicator
    float curX = (dur > 0.0f)
        ? trackTL.x + std::clamp(animTime_ / dur, 0.0f, 1.0f) * trackW
        : trackTL.x;
    dl->AddLine(ImVec2(curX, trackTL.y),
                ImVec2(curX, trackTL.y + availH),
                IM_COL32(255, 200, 50, 220), 2.0f);

    // Click on ruler → seek
    ImGui::SetCursorScreenPos(trackTL);
    ImGui::InvisibleButton("##ruler", ImVec2(trackW, rowH));
    if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        float mx = ImGui::GetIO().MousePos.x;
        animTime_ = std::clamp((mx - trackTL.x) / trackW * dur, 0.0f, dur);
        evaluateAndPushAnimOverrides();
    }

    // Channel rows with keyframe dots (drag-to-move + click-to-select)
    if (hasAct) {
        auto& act = document_.actions[currentActionName_];
        // Clamp selection if action changed
        if (tlSelChan_ >= (int)act.channels.size()) { tlSelChan_ = -1; tlSelKf_ = -1; }

        ImVec2 mp = ImGui::GetIO().MousePos;
        bool mbDown    = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool mbPressed = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool mbReleased= ImGui::IsMouseReleased(ImGuiMouseButton_Left);

        // Handle active drag (single or group)
        if (tlDragChan_ >= 0 && tlDragChan_ < (int)act.channels.size()) {
            auto& dch = act.channels[tlDragChan_];
            if (tlDragKf_ >= 0 && tlDragKf_ < (int)dch.keyframes.size()) {
                if (mbDown) {
                    float newT = std::clamp((mp.x - trackTL.x) / std::max(trackW, 1.0f) * dur,
                                            0.0f, dur);
                    if (tlGroupDrag_ && !tlMultiSel_.empty()) {
                        float delta = newT - tlGroupDragPrev_;
                        tlGroupDragPrev_ = newT;
                        for (const auto& [sc, sk] : tlMultiSel_) {
                            if (sc < (int)act.channels.size() &&
                                sk < (int)act.channels[sc].keyframes.size())
                                act.channels[sc].keyframes[sk].time =
                                    std::clamp(act.channels[sc].keyframes[sk].time + delta,
                                               0.0f, dur);
                        }
                    } else {
                        dch.keyframes[tlDragKf_].time = newT;
                    }
                    animTime_ = newT;
                    evaluateAndPushAnimOverrides();
                } else if (mbReleased) {
                    // Save times to rebuild indices after sort
                    std::vector<std::pair<int,float>> savedTimes;
                    if (tlGroupDrag_) {
                        for (const auto& [sc, sk] : tlMultiSel_)
                            if (sc < (int)act.channels.size() &&
                                sk < (int)act.channels[sc].keyframes.size())
                                savedTimes.push_back({sc, act.channels[sc].keyframes[sk].time});
                    }
                    float movedTime = dch.keyframes[tlDragKf_].time;
                    std::set<int> chansToSort;
                    if (tlGroupDrag_) for (const auto& [sc, sk] : tlMultiSel_) chansToSort.insert(sc);
                    else chansToSort.insert(tlDragChan_);
                    for (int sc : chansToSort)
                        if (sc < (int)act.channels.size())
                            std::stable_sort(act.channels[sc].keyframes.begin(),
                                             act.channels[sc].keyframes.end(),
                                [](const Mc3::Mc3Keyframe& a, const Mc3::Mc3Keyframe& b){
                                    return a.time < b.time; });
                    if (tlGroupDrag_) {
                        tlMultiSel_.clear();
                        for (const auto& [sc, t] : savedTimes) {
                            if (sc >= (int)act.channels.size()) continue;
                            for (int i = 0; i < (int)act.channels[sc].keyframes.size(); ++i)
                                if (std::abs(act.channels[sc].keyframes[i].time - t) < 1e-5f) {
                                    tlMultiSel_.insert({sc, i}); break;
                                }
                        }
                    } else {
                        tlSelChan_ = tlDragChan_; tlSelKf_ = 0;
                        for (int i = 0; i < (int)dch.keyframes.size(); ++i)
                            if (std::abs(dch.keyframes[i].time - movedTime) < 1e-5f) { tlSelKf_ = i; break; }
                    }
                    tlDragChan_ = -1; tlDragKf_ = -1; tlGroupDrag_ = false;
                    modified_ = true;
                }
            }
        }

        // Finalize box select
        if (tlBoxActive_ && !mbDown) {
            float bx0 = std::min(tlBoxX0_, tlBoxX1_), bx1 = std::max(tlBoxX0_, tlBoxX1_);
            float by0 = std::min(tlBoxY0_, tlBoxY1_), by1 = std::max(tlBoxY0_, tlBoxY1_);
            if (!ImGui::GetIO().KeyShift) tlMultiSel_.clear();
            for (int ci = 0; ci < (int)act.channels.size(); ++ci) {
                float rowY = trackTL.y + rowH + ci * rowH;
                float ky = rowY + rowH * 0.5f;
                if (ky < by0 || ky > by1) continue;
                for (int ki = 0; ki < (int)act.channels[ci].keyframes.size(); ++ki) {
                    float kx = trackTL.x + (act.channels[ci].keyframes[ki].time /
                                            std::max(dur, 0.001f)) * trackW;
                    if (kx >= bx0 && kx <= bx1) tlMultiSel_.insert({ci, ki});
                }
            }
            tlBoxActive_ = false;
        }
        if (tlBoxActive_) { tlBoxX1_ = mp.x; tlBoxY1_ = mp.y; }

        int kfDelChan = -1, kfDelIdx = -1;
        bool anyKfHit = false;

        for (int ci = 0; ci < (int)act.channels.size(); ++ci) {
            float rowY = trackTL.y + rowH + ci * rowH;
            ImU32 rowBg = (ci % 2 == 0) ? IM_COL32(18, 20, 34, 255)
                                         : IM_COL32(23, 26, 42, 255);
            dl->AddRectFilled(ImVec2(trackTL.x, rowY),
                              ImVec2(trackTL.x + trackW, rowY + rowH), rowBg);

            auto& ch = act.channels[ci];

            // Mini curve — sample evaluateChannel across track width
            if (!ch.keyframes.empty() && dur > 0.001f) {
                float vmin = ch.keyframes[0].value, vmax = vmin;
                for (const auto& kf : ch.keyframes) {
                    vmin = std::min(vmin, kf.value);
                    vmax = std::max(vmax, kf.value);
                }
                float vrange = vmax - vmin;
                if (vrange < 1e-5f) { vmin -= 0.5f; vrange = 1.0f; }
                const float marg   = 2.0f;
                const float usable = rowH - 2.0f * marg;
                const int   ns     = std::min(160, std::max(2, static_cast<int>(trackW / 3)));
                ImVec2 prev{};
                for (int si = 0; si < ns; ++si) {
                    float t  = static_cast<float>(si) / (ns - 1) * dur;
                    float v  = Mc3::evaluateChannel(ch, t);
                    float sx = trackTL.x + (t / dur) * trackW;
                    float sy = rowY + marg + usable * (1.0f - (v - vmin) / vrange);
                    ImVec2 cur(sx, sy);
                    if (si > 0) dl->AddLine(prev, cur, IM_COL32(70, 200, 95, 140), 1.0f);
                    prev = cur;
                }
            }

            for (int ki = 0; ki < (int)ch.keyframes.size(); ++ki) {
                float kx = trackTL.x + (ch.keyframes[ki].time / std::max(dur, 0.001f)) * trackW;
                float ky = rowY + rowH * 0.5f;
                float dx = mp.x - kx, dy = mp.y - ky;
                bool hov = (dx*dx + dy*dy) < 36.0f;
                bool isSel = (tlSelChan_ == ci && tlSelKf_ == ki) ||
                             tlMultiSel_.count({ci, ki});
                bool isDrag = (tlDragChan_ == ci && tlDragKf_ == ki);

                ImU32 col = isDrag ? IM_COL32(255, 240, 100, 255)
                          : isSel  ? IM_COL32(100, 210, 255, 255)
                          : hov    ? IM_COL32(255, 215,  80, 255)
                                   : IM_COL32(200, 155,  50, 255);
                dl->AddCircleFilled(ImVec2(kx, ky), 5.5f, col);
                dl->AddCircle(ImVec2(kx, ky), 6.0f, IM_COL32(255, 255, 200, 160));

                if (hov && mbPressed && tlDragChan_ < 0 && !tlBoxActive_) {
                    anyKfHit = true;
                    if (ImGui::GetIO().KeyShift) {
                        auto key = std::make_pair(ci, ki);
                        if (tlMultiSel_.count(key)) tlMultiSel_.erase(key);
                        else { tlMultiSel_.insert(key); tlSelChan_ = ci; tlSelKf_ = ki; }
                    } else {
                        pushUndo();
                        tlDragChan_ = ci; tlDragKf_ = ki;
                        animTime_ = ch.keyframes[ki].time;
                        evaluateAndPushAnimOverrides();
                        if (!tlMultiSel_.empty() && tlMultiSel_.count({ci, ki})) {
                            tlGroupDrag_ = true;
                            tlGroupDragPrev_ = ch.keyframes[ki].time;
                        } else {
                            tlMultiSel_.clear(); tlGroupDrag_ = false;
                            tlSelChan_ = ci; tlSelKf_ = ki;
                        }
                    }
                }
                if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    anyKfHit = true;
                    kfDelChan = ci; kfDelIdx = ki;
                }
            }
        }

        // Start box select on empty area
        if (mbPressed && !anyKfHit && !tlBoxActive_ && tlDragChan_ < 0) {
            if (mp.y > trackTL.y + rowH && mp.x > trackTL.x && mp.x < trackTL.x + trackW) {
                tlBoxX0_ = tlBoxX1_ = mp.x;
                tlBoxY0_ = tlBoxY1_ = mp.y;
                tlBoxActive_ = true;
                if (!ImGui::GetIO().KeyShift) { tlMultiSel_.clear(); tlSelChan_ = -1; tlSelKf_ = -1; }
            }
        }

        // Draw box select rectangle
        if (tlBoxActive_) {
            float bx0 = std::min(tlBoxX0_, tlBoxX1_), bx1 = std::max(tlBoxX0_, tlBoxX1_);
            float by0 = std::min(tlBoxY0_, tlBoxY1_), by1 = std::max(tlBoxY0_, tlBoxY1_);
            dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx1, by1), IM_COL32(100, 180, 255, 40));
            dl->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1), IM_COL32(100, 180, 255, 200));
        }

        // Ctrl+C: copy selected keyframes to clipboard
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
            std::vector<std::pair<int,int>> toCopy;
            if (!tlMultiSel_.empty())
                toCopy.assign(tlMultiSel_.begin(), tlMultiSel_.end());
            else if (tlSelChan_ >= 0 && tlSelKf_ >= 0 &&
                     tlSelChan_ < (int)act.channels.size() &&
                     tlSelKf_ < (int)act.channels[tlSelChan_].keyframes.size())
                toCopy.push_back({tlSelChan_, tlSelKf_});
            if (!toCopy.empty()) {
                float minT = 1e30f;
                for (const auto& [sc, sk] : toCopy)
                    if (sc < (int)act.channels.size() && sk < (int)act.channels[sc].keyframes.size())
                        minT = std::min(minT, act.channels[sc].keyframes[sk].time);
                kfClipboard_.clear();
                for (const auto& [sc, sk] : toCopy) {
                    if (sc >= (int)act.channels.size()) continue;
                    const auto& ch = act.channels[sc];
                    if (sk >= (int)ch.keyframes.size()) continue;
                    const auto& kf = ch.keyframes[sk];
                    KfClipEntry e;
                    e.targetObject = ch.targetObject;
                    e.property     = ch.property;
                    e.relTime      = kf.time - minT;
                    e.value        = kf.value;
                    e.interpolation = kf.interpolation;
                    e.handleLeft   = kf.handleLeft;
                    e.handleRight  = kf.handleRight;
                    kfClipboard_.push_back(e);
                }
            }
        }

        // Ctrl+V: paste clipboard keyframes at animTime_
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && !kfClipboard_.empty()) {
            pushUndo();
            tlMultiSel_.clear();
            for (const auto& e : kfClipboard_) {
                int ci = -1;
                for (int i = 0; i < (int)act.channels.size(); ++i)
                    if (act.channels[i].targetObject == e.targetObject &&
                        act.channels[i].property == e.property) { ci = i; break; }
                if (ci < 0) {
                    Mc3::Mc3Channel ch;
                    ch.targetObject = e.targetObject;
                    ch.property     = e.property;
                    act.channels.push_back(std::move(ch));
                    ci = static_cast<int>(act.channels.size()) - 1;
                }
                Mc3::Mc3Keyframe kf;
                kf.time          = std::clamp(animTime_ + e.relTime, 0.0f, dur);
                kf.value         = e.value;
                kf.interpolation = e.interpolation;
                kf.handleLeft    = e.handleLeft;
                kf.handleRight   = e.handleRight;
                act.channels[ci].keyframes.push_back(kf);
                float pastedTime = kf.time;
                float pastedVal  = kf.value;
                std::stable_sort(act.channels[ci].keyframes.begin(),
                                 act.channels[ci].keyframes.end(),
                    [](const Mc3::Mc3Keyframe& a, const Mc3::Mc3Keyframe& b){
                        return a.time < b.time; });
                for (int i = 0; i < (int)act.channels[ci].keyframes.size(); ++i)
                    if (std::abs(act.channels[ci].keyframes[i].time - pastedTime) < 1e-5f &&
                        act.channels[ci].keyframes[i].value == pastedVal) {
                        tlMultiSel_.insert({ci, i}); break;
                    }
            }
            modified_ = true;
            evaluateAndPushAnimOverrides();
        }

        // Delete key: remove all selected keyframes
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !tlMultiSel_.empty()) {
            pushUndo();
            std::vector<std::pair<int,int>> toRemove(tlMultiSel_.begin(), tlMultiSel_.end());
            std::sort(toRemove.begin(), toRemove.end(), [](const auto& a, const auto& b){
                return a.first != b.first ? a.first > b.first : a.second > b.second; });
            for (const auto& [sc, sk] : toRemove) {
                if (sc < (int)act.channels.size() && sk < (int)act.channels[sc].keyframes.size()) {
                    act.channels[sc].keyframes.erase(act.channels[sc].keyframes.begin() + sk);
                    if (act.channels[sc].keyframes.empty())
                        act.channels.erase(act.channels.begin() + sc);
                }
            }
            tlMultiSel_.clear(); tlSelChan_ = -1; tlSelKf_ = -1;
            modified_ = true; evaluateAndPushAnimOverrides();
        }

        if (kfDelChan >= 0) {
            pushUndo();
            act.channels[kfDelChan].keyframes.erase(
                act.channels[kfDelChan].keyframes.begin() + kfDelIdx);
            if (act.channels[kfDelChan].keyframes.empty())
                act.channels.erase(act.channels.begin() + kfDelChan);
            if (tlSelChan_ == kfDelChan) { tlSelChan_ = -1; tlSelKf_ = -1; }
            tlMultiSel_.erase({kfDelChan, kfDelIdx});
            modified_ = true; evaluateAndPushAnimOverrides();
        }
    }

    ImGui::EndChild();

    // ----- Selected keyframe info bar -----
    if (hasAct && !tlMultiSel_.empty() && tlMultiSel_.size() > 1) {
        ImGui::Separator();
        ImGui::Text("%d keyframes selected", static_cast<int>(tlMultiSel_.size()));
        ImGui::SameLine();
        if (ImGui::SmallButton("Desel")) { tlMultiSel_.clear(); }
    }
    if (hasAct && tlSelChan_ >= 0) {
        auto& act = document_.actions[currentActionName_];
        if (tlSelChan_ < (int)act.channels.size()) {
            auto& ch = act.channels[tlSelChan_];
            if (tlSelKf_ < (int)ch.keyframes.size()) {
                auto& kf = ch.keyframes[tlSelKf_];
                ImGui::Separator();
                ImGui::Text("KF  t=");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(70.0f);
                float t = kf.time;
                if (ImGui::DragFloat("##kft", &t, 0.001f, 0.0f,
                                     act.duration, "%.3f")) {
                    kf.time = std::clamp(t, 0.0f, act.duration);
                    std::stable_sort(ch.keyframes.begin(), ch.keyframes.end(),
                        [](const Mc3::Mc3Keyframe& a, const Mc3::Mc3Keyframe& b){
                            return a.time < b.time; });
                    animTime_ = kf.time;
                    evaluateAndPushAnimOverrides();
                    modified_ = true;
                }
                ImGui::SameLine();
                ImGui::Text("val=");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::DragFloat("##kfv", &kf.value, 0.01f)) {
                    evaluateAndPushAnimOverrides(); modified_ = true;
                }
                ImGui::SameLine();
                const char* interpNames[] = {"Step", "Linear", "Cubic"};
                int interp = static_cast<int>(kf.interpolation);
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::Combo("##kfinterp", &interp, interpNames, 3)) {
                    kf.interpolation = static_cast<Mc3::Interpolation>(interp);
                    evaluateAndPushAnimOverrides(); modified_ = true;
                }
                if (kf.interpolation == Mc3::Interpolation::CubicBezier) {
                    ImGui::SameLine();
                    ImGui::Text("L(dt=");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f);
                    if (ImGui::DragFloat("##ldt", &kf.handleLeft.dt, 0.001f))
                        { evaluateAndPushAnimOverrides(); modified_ = true; }
                    ImGui::SameLine();
                    ImGui::Text("dv=");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f);
                    if (ImGui::DragFloat("##ldv", &kf.handleLeft.dv, 0.01f))
                        { evaluateAndPushAnimOverrides(); modified_ = true; }
                    ImGui::SameLine();
                    ImGui::Text(") R(dt=");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f);
                    if (ImGui::DragFloat("##rdt", &kf.handleRight.dt, 0.001f))
                        { evaluateAndPushAnimOverrides(); modified_ = true; }
                    ImGui::SameLine();
                    ImGui::Text("dv=");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f);
                    if (ImGui::DragFloat("##rdv", &kf.handleRight.dv, 0.01f))
                        { evaluateAndPushAnimOverrides(); modified_ = true; }
                    ImGui::SameLine();
                    ImGui::Text(")");
                }
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace MeshCraft
