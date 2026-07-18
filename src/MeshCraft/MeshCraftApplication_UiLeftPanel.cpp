#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"
#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"

#include <imgui.h>

#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/Viewport.hpp>
#include <Microsoft/Xna/Framework/Audio/SoundEffect.hpp>
#include <Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp>
#include <Microsoft/Xna/Framework/Audio/SoundState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace MeshCraft {

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Graphics;


// STAB-0706: preview playback for Mc3Sound/Mc3Music entries, via CNA's
// SoundEffect/SoundEffectInstance (Microsoft::Xna::Framework::Audio). One
// shared preview at a time -- starting a new one stops whatever was
// playing. Loop must be set before the first Play() call (setIsLoopedProperty
// throws InvalidOperationException once playback has started), so it's
// applied here, before Play(), not toggleable afterward.
void MeshCraftApplication::playAudioPreview(const std::string& key, const std::string& srcPath, bool loop)
{
    using namespace Microsoft::Xna::Framework::Audio;

    stopAudioPreview();
    audioPreviewError_.clear();

    try {
        SoundEffect se(srcPath);
        audioPreviewInstance_ = std::make_unique<SoundEffectInstance>(se.CreateInstance());
        audioPreviewInstance_->setIsLoopedProperty(loop);
        audioPreviewInstance_->Play();
        audioPreviewKey_ = key;
    } catch (const std::exception& e) {
        audioPreviewInstance_.reset();
        audioPreviewKey_.clear();
        audioPreviewError_ = std::string("Playback failed: ") + e.what();
    }
}

void MeshCraftApplication::stopAudioPreview()
{
    if (audioPreviewInstance_) {
        audioPreviewInstance_->Stop();
        audioPreviewInstance_.reset();
    }
    audioPreviewKey_.clear();
}

void MeshCraftApplication::drawLeftPanel(float panelY, float panelH)
{
    int tlPanelH = showTimeline_ ? kTimelineH : 0;
    (void)tlPanelH;
    // -----------------------------------------------------------------------
    // Left panel — tabbed (Scene / Lights)
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(0, panelY));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kLeftPanelW), panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.155f, 0.155f, 0.155f, 1.0f));
    ImGui::Begin("##leftpanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    // FittingPolicyScroll: with 12 tabs now (5 added by S24 on 2026-07-10:
    // Scripts/Audio/Triggers/Embeds/States), the default shrink-to-fit
    // policy squeezed every label down to an unreadable sliver -- found via
    // live verification. Scroll arrows keep each tab's full label legible.
    if (ImGui::BeginTabBar("##lefttabs", ImGuiTabBarFlags_FittingPolicyScroll)) {

        // -------------------------------------------------------------------
        // Tab: Scene hierarchy  (L1 — delegated to SceneHierarchyPanel)
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Scene")) {
            Scene::HierarchyCallbacks cb;
            cb.pushUndo       = [this]() { pushUndo(); };
            cb.markModified   = [this]() { modified_ = true; updateWindowTitle(); };
            cb.duplicateSel   = [this]() { duplicateSelected(); };
            cb.deleteSel      = [this]() { deleteSelected(); };
            cb.selectParent   = [this]() { selectParent(); };
            cb.selectChildren = [this]() { selectChildren(); };
            cb.openBatchRename = [this]() { batchRenameOpen_ = true; };
            hierarchyPanel_->draw(selection_, lockedIds_, cb);
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
                // AUDIT-0059: these light-property widgets set modified_
                // (unsaved-changes indicator) without pushUndo(), unlike the
                // Name/Type fields above -- so the user sees "unsaved
                // changes" but Ctrl+Z can't revert the edit. Added pushUndo(),
                // gated on IsItemActivated() for continuous-drag widgets
                // (Drag/Slider/ColorEdit fire every frame during a drag, so
                // an unconditional pushUndo() would push one snapshot per
                // frame and capture intermediate values, not the pre-edit
                // state -- matches the established pattern already used
                // elsewhere in this file, e.g. the camera near/far fields).
                // Checkbox fires once per click, so it's safe unconditional.
                ImGui::TextDisabled("Color");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh190 = ImGui::ColorEdit3("##lcol", li.color.data(),
                        ImGuiColorEditFlags_NoLabel);
                    if (ImGui::IsItemActivated()) pushUndo();
                    if (_undoCh190) {
                    modified_ = true; updateWindowTitle();
                } }

                // Brightness
                ImGui::TextDisabled("Brightness");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp (AUDIT-0043): bounded light/camera/fog params
                // below have no other downstream guard against an
                // out-of-range Ctrl+Click-typed value; unbounded ones
                // (position/rotation/direction DragFloat3 calls with no
                // explicit min/max) are deliberately left untouched.
                { bool _undoCh204 = ImGui::DragFloat("##lbrt", &li.brightness, 0.01f, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) pushUndo();
                if (_undoCh204) {
                    modified_ = true; updateWindowTitle();
                } }

                // Cast shadows
                if (ImGui::Checkbox("Cast Shadows", &li.castShadows)) {
                    pushUndo(); modified_ = true; updateWindowTitle();
                }

                // Direction (Directional / Spot)
                if (li.type == Mc3::LightType::Directional || li.type == Mc3::LightType::Spot) {
                    ImGui::TextDisabled("Direction");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh218 = ImGui::DragFloat3("##ldir", li.direction.data(), 0.01f, -1.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) pushUndo();
                    if (_undoCh218) {
                        modified_ = true; updateWindowTitle();
                    } }
                }

                // Position (Spot / Point)
                if (li.type == Mc3::LightType::Spot || li.type == Mc3::LightType::Point) {
                    ImGui::TextDisabled("Position");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh228 = ImGui::DragFloat3("##lpos", li.position.data(), 0.1f);
                    if (ImGui::IsItemActivated()) pushUndo();
                    if (_undoCh228) {
                        modified_ = true; updateWindowTitle();
                    } }
                    ImGui::TextDisabled("Range (0=unlimited)");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh234 = ImGui::DragFloat("##lrng", &li.range, 0.1f, 0.0f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) pushUndo();
                    if (_undoCh234) {
                        modified_ = true; updateWindowTitle();
                    } }
                }

                // Spot-only params
                if (li.type == Mc3::LightType::Spot) {
                    ImGui::TextDisabled("Angle (deg)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##lang", &li.angle, 0.0f, 90.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Falloff");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##lfal", &li.falloff, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
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
                // AUDIT-0059: missing pushUndo(), same class of gap as the
                // light editor above -- IsItemActivated()-gated to avoid a
                // per-frame push during a continuous color-picker drag.
                ImGui::TextDisabled("Background Color");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh288 = ImGui::ColorEdit3("##envbg", env.backgroundColor.data(),
                        ImGuiColorEditFlags_NoLabel);
                    if (ImGui::IsItemActivated()) pushUndo();
                    if (_undoCh288) {
                    modified_ = true; updateWindowTitle();
                } }

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

                // Skybox texture (equirectangular panorama)
                ImGui::TextDisabled("Skybox Texture (equirect)");
                {
                    char buf[256];
                    std::strncpy(buf, env.skyboxTexture.c_str(), sizeof(buf)-1); buf[255]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##envskybox", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); env.skyboxTexture = buf;
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
                    { bool _undoCh338 = ImGui::ColorEdit3("##fogcol", fog.color.data(),
                            ImGuiColorEditFlags_NoLabel);
                        if (ImGui::IsItemActivated()) pushUndo();
                        if (_undoCh338) {
                        modified_ = true; updateWindowTitle();
                    } }

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
                        { bool _undoCh353 = ImGui::DragFloat("##fogstart", &fog.start, 0.5f, 0.0f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                        if (ImGui::IsItemActivated()) pushUndo();
                        if (_undoCh353) {
                            modified_ = true; updateWindowTitle();
                        } }
                        ImGui::TextDisabled("End");
                        ImGui::SetNextItemWidth(-1);
                        { bool _undoCh359 = ImGui::DragFloat("##fogend", &fog.end, 0.5f, 0.0f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                        if (ImGui::IsItemActivated()) pushUndo();
                        if (_undoCh359) {
                            modified_ = true; updateWindowTitle();
                        } }
                    } else {
                        ImGui::TextDisabled("Density");
                        ImGui::SetNextItemWidth(-1);
                        { bool _undoCh366 = ImGui::DragFloat("##fogdens", &fog.density, 0.001f, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
                        if (ImGui::IsItemActivated()) pushUndo();
                        if (_undoCh366) {
                            modified_ = true; updateWindowTitle();
                        } }
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
                { bool _undoCh467 = ImGui::DragFloat3("##cpos", cam.position.data(), 0.1f);
                if (ImGui::IsItemActivated()) pushUndo();
                if (_undoCh467) {
                    modified_ = true; updateWindowTitle();
                } }

                // Target
                ImGui::TextDisabled("Target");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh475 = ImGui::DragFloat3("##ctgt", cam.target.data(), 0.1f);
                if (ImGui::IsItemActivated()) pushUndo();
                if (_undoCh475) {
                    modified_ = true; updateWindowTitle();
                } }

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
                        { bool _undoCh491 = ImGui::DragFloat3("##crot", cam.rotation->data(), 0.5f);
                        if (ImGui::IsItemActivated()) pushUndo();
                        if (_undoCh491) {
                            modified_ = true; updateWindowTitle();
                        } }
                    }
                }

                // Near / Far
                ImGui::TextDisabled("Near Plane");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: keeps the near/far invariant (near <= far)
                // enforced even against a Ctrl+Click typed value, not just
                // the drag gesture.
                { bool _undoCh504 = ImGui::DragFloat("##cnear", &cam.nearPlane, 0.01f, 0.001f, cam.farPlane, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) pushUndo();
                if (_undoCh504) {
                    modified_ = true; updateWindowTitle();
                } }
                ImGui::TextDisabled("Far Plane");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh510 = ImGui::DragFloat("##cfar", &cam.farPlane, 1.0f, cam.nearPlane, 100000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActivated()) pushUndo();
                if (_undoCh510) {
                    modified_ = true; updateWindowTitle();
                } }

                // Perspective-only: FOV
                if (cam.type == Mc3::CameraType::Perspective) {
                    ImGui::TextDisabled("FOV (deg)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##cfov", &cam.fov, 1.0f, 170.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Orthographic-only: ortho size
                if (cam.type == Mc3::CameraType::Orthographic) {
                    ImGui::TextDisabled("Ortho Size");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh529 = ImGui::DragFloat("##cortho", &cam.orthoSize, 0.1f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) pushUndo();
                    if (_undoCh529) {
                        modified_ = true; updateWindowTitle();
                    } }
                    // STAB-0695: width/height ratio baked into the exported
                    // glTF orthographic camera (xmag = size * aspect, ymag =
                    // size). Doesn't affect the editor's own "look through
                    // camera" preview, which already fits the live viewport.
                    ImGui::TextDisabled("Aspect (export)");
                    ImGui::SetNextItemWidth(-1);
                    { bool _undoCh539 = ImGui::DragFloat("##corthoaspect", &cam.orthoAspect, 0.01f, 0.01f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemActivated()) pushUndo();
                    if (_undoCh539) {
                        modified_ = true; updateWindowTitle();
                    } }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Width/height ratio of the exported orthographic view volume (xmag = size x aspect). 1.0 = square.");
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

            // STAB-0703: SVG textures (N1, doc.svgTextures) previously had
            // zero editor UI at all -- a user could only assign/create one
            // by hand-editing XML. Mirrors the raster-texture list/editor
            // pattern above, in the same tab since both are "textures" from
            // a user's perspective.
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("SVG Textures");
            ImGui::Separator();

            if (!selectedSvgTextureKey_.empty() &&
                !document_.svgTextures.count(selectedSvgTextureKey_))
                selectedSvgTextureKey_.clear();

            if (ImGui::SmallButton("+##svgadd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "svg_" + std::to_string(n++); }
                while (document_.svgTextures.count(key));
                Mc3::Mc3SvgTexture svg;
                svg.id = key;
                document_.svgTextures[key] = svg;
                selectedSvgTextureKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-##svgremove") && !selectedSvgTextureKey_.empty()) {
                pushUndo();
                document_.svgTextures.erase(selectedSvgTextureKey_);
                selectedSvgTextureKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            for (const auto& [key, svg] : document_.svgTextures) {
                bool sel = (key == selectedSvgTextureKey_);
                std::string label = key;
                if (!svg.src.empty()) {
                    auto slash = svg.src.find_last_of("/\\");
                    label += "  " + (slash != std::string::npos ? svg.src.substr(slash+1) : svg.src);
                } else if (!svg.inlineContent.empty()) {
                    label += "  (inline)";
                }
                ImGui::PushID(("svg_" + key).c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedSvgTextureKey_ = key;
                ImGui::PopID();
            }

            if (!selectedSvgTextureKey_.empty() &&
                document_.svgTextures.count(selectedSvgTextureKey_))
            {
                auto& svg = document_.svgTextures[selectedSvgTextureKey_];
                ImGui::Spacing();

                ImGui::TextDisabled("ID");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##svgid"))
                    ImGui::SetClipboardText(selectedSvgTextureKey_.c_str());
                ImGui::TextUnformatted(selectedSvgTextureKey_.c_str());

                // External vs inline are mutually exclusive per
                // Mc3SvgTexture's own contract (src empty <=> inline set).
                bool isInline = svg.isInline();
                ImGui::TextDisabled("Type");
                if (ImGui::RadioButton("External##svgtype", !isInline)) {
                    if (isInline) {
                        pushUndo(); svg.inlineContent.clear(); modified_ = true; updateWindowTitle();
                    }
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Inline##svgtype", isInline)) {
                    if (!isInline) {
                        pushUndo(); svg.src.clear();
                        // isInline() requires inlineContent to be non-empty
                        // (src empty && !inlineContent.empty()) -- without
                        // seeding it, the radio button could never actually
                        // flip to Inline for a fresh/External entry.
                        if (svg.inlineContent.empty())
                            svg.inlineContent = "<svg xmlns=\"http://www.w3.org/2000/svg\"></svg>";
                        modified_ = true; updateWindowTitle();
                    }
                }

                if (!isInline) {
                    ImGui::TextDisabled("Source path (.svg)");
                    char buf[512];
                    std::strncpy(buf, svg.src.c_str(), sizeof(buf)-1); buf[511]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##svgsrc", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); svg.src = buf; modified_ = true; updateWindowTitle();
                    }
                } else {
                    ImGui::TextDisabled("Inline SVG markup");
                    char buf[8192];
                    std::strncpy(buf, svg.inlineContent.c_str(), sizeof(buf)-1); buf[8191]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputTextMultiline("##svginline", buf, sizeof(buf),
                            ImVec2(-1, 120), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); svg.inlineContent = buf; modified_ = true; updateWindowTitle();
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
                                        if (o->type == Mc3::ObjectType::Instance) {
                                            if (o->definition == selectedDefId_)
                                                o->definition = newKey;
                                            for (auto& v : o->variantDefinitions)
                                                if (v == selectedDefId_) v = newKey;
                                        }
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
                        { bool _undoCh926 = ImGui::DragFloat3("##dpos", pos, 0.1f);
                        if (ImGui::IsItemActivated()) pushUndo();
                        if (_undoCh926) {
                            defObj->transform.position[0] = pos[0];
                            defObj->transform.position[1] = pos[1];
                            defObj->transform.position[2] = pos[2];
                            modified_ = true; updateWindowTitle();
                        } }
                    }
                    ImGui::TextDisabled("Rotation");
                    {
                        float rot[3] = { defObj->transform.rotation[0], defObj->transform.rotation[1], defObj->transform.rotation[2] };
                        ImGui::SetNextItemWidth(-1);
                        { bool _undoCh938 = ImGui::DragFloat3("##drot", rot, 0.5f);
                        if (ImGui::IsItemActivated()) pushUndo();
                        if (_undoCh938) {
                            defObj->transform.rotation[0] = rot[0];
                            defObj->transform.rotation[1] = rot[1];
                            defObj->transform.rotation[2] = rot[2];
                            modified_ = true; updateWindowTitle();
                        } }
                    }
                    ImGui::TextDisabled("Scale");
                    {
                        float scl[3] = { defObj->transform.scale[0], defObj->transform.scale[1], defObj->transform.scale[2] };
                        ImGui::SetNextItemWidth(-1);
                        // AlwaysClamp: the callback already clamps scale
                        // defensively (std::max below), this keeps the
                        // widget's own displayed value consistent within
                        // the same frame.
                        { bool _undoCh954 = ImGui::DragFloat3("##dscl", scl, 0.01f, 0.001f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                        if (ImGui::IsItemActivated()) pushUndo();
                        if (_undoCh954) {
                            defObj->transform.scale[0] = std::max(0.001f, scl[0]);
                            defObj->transform.scale[1] = std::max(0.001f, scl[1]);
                            defObj->transform.scale[2] = std::max(0.001f, scl[2]);
                            modified_ = true; updateWindowTitle();
                        } }
                    }

                    // SYS-W14-12: Mc3Object::assetMetadata (R111) had zero
                    // editor UI. Present only on definitions (per its own
                    // header comment), so this is the right home -- the Defs
                    // tab already edits document_.definitions[selectedDefId_].
                    // Collapsed by default (23 fields across 6 shapes) so it
                    // doesn't dominate this tab when not in use.
                    ImGui::Spacing();
                    ImGui::Separator();
                    if (ImGui::TreeNode("Asset Metadata (R111)")) {
                        bool hasMeta = defObj->assetMetadata.has_value();
                        if (ImGui::Checkbox("Has asset metadata", &hasMeta)) {
                            pushUndo();
                            if (hasMeta) defObj->assetMetadata = Mc3::Mc3AssetMetadata{};
                            else         defObj->assetMetadata.reset();
                            modified_ = true; updateWindowTitle();
                        }
                        if (hasMeta) {
                            auto& am = *defObj->assetMetadata;

                            auto strField = [&](const char* label, std::string& field, size_t bufSize) {
                                ImGui::TextDisabled("%s", label);
                                std::vector<char> buf(bufSize);
                                std::strncpy(buf.data(), field.c_str(), bufSize - 1);
                                buf[bufSize - 1] = '\0';
                                ImGui::SetNextItemWidth(-1);
                                ImGui::PushID(label);
                                if (ImGui::InputText("##amstr", buf.data(), bufSize,
                                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                                    pushUndo(); field = buf.data(); modified_ = true; updateWindowTitle();
                                }
                                ImGui::PopID();
                            };
                            auto vec3Field = [&](const char* label, std::array<float, 3>& v) {
                                ImGui::TextDisabled("%s", label);
                                float f[3] = { v[0], v[1], v[2] };
                                ImGui::SetNextItemWidth(-1);
                                ImGui::PushID(label);
                                bool changed = ImGui::DragFloat3("##amvec3", f, 0.01f);
                                if (ImGui::IsItemActivated()) pushUndo();
                                if (changed) {
                                    v = {f[0], f[1], f[2]};
                                    modified_ = true; updateWindowTitle();
                                }
                                ImGui::PopID();
                            };
                            // Comma-separated single-line editor for a tag
                            // list -- simpler than a full per-item add/
                            // remove UI, proportionate to there being 5 of
                            // these (4 tag categories + materialSlots).
                            auto tagListField = [&](const char* label, std::vector<std::string>& tags) {
                                ImGui::TextDisabled("%s (comma-separated)", label);
                                std::string joined;
                                for (size_t i = 0; i < tags.size(); ++i) {
                                    if (i) joined += ", ";
                                    joined += tags[i];
                                }
                                char buf[512];
                                std::strncpy(buf, joined.c_str(), sizeof(buf) - 1); buf[511] = '\0';
                                ImGui::SetNextItemWidth(-1);
                                ImGui::PushID(label);
                                if (ImGui::InputText("##amtags", buf, sizeof(buf),
                                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                                    pushUndo();
                                    tags.clear();
                                    std::string cur;
                                    std::istringstream iss(std::string(buf) + ",");
                                    while (std::getline(iss, cur, ',')) {
                                        size_t b = cur.find_first_not_of(" \t");
                                        size_t e = cur.find_last_not_of(" \t");
                                        if (b != std::string::npos) tags.push_back(cur.substr(b, e - b + 1));
                                    }
                                    modified_ = true; updateWindowTitle();
                                }
                                ImGui::PopID();
                            };

                            strField("Category", am.category, 128);
                            strField("Subcategory", am.subcategory, 128);
                            tagListField("Semantic Tags", am.semanticTags);
                            tagListField("Style Tags", am.styleTags);
                            tagListField("Region Tags", am.regionTags);
                            tagListField("Period Tags", am.periodTags);
                            vec3Field("Nominal Size", am.nominalSize);
                            vec3Field("Bounds Min", am.boundsMin);
                            vec3Field("Bounds Max", am.boundsMax);
                            strField("Facing (e.g. -Z, +X)", am.facing, 16);
                            tagListField("Material Slots", am.materialSlots);
                            strField("Collision Proxy (box/convex_hull/none)", am.collisionProxy, 64);
                            vec3Field("Clearance Volume", am.clearanceVolume);

                            ImGui::Checkbox("Instancing Eligible", &am.instancingEligible);
                            if (ImGui::IsItemDeactivatedAfterEdit()) { pushUndo(); modified_ = true; updateWindowTitle(); }

                            strField("Shadow Policy (cast_receive/cast_only/none)", am.shadowPolicy, 32);

                            ImGui::TextDisabled("Max Visibility Distance (m, 0=unlimited)");
                            ImGui::SetNextItemWidth(-1);
                            { bool ch = ImGui::DragFloat("##ammaxvis", &am.maxVisibilityDistanceM, 1.0f, 0.0f, 100000.0f);
                              if (ImGui::IsItemActivated()) pushUndo();
                              if (ch) { modified_ = true; updateWindowTitle(); } }

                            ImGui::TextDisabled("Selection Weight (higher = more common)");
                            ImGui::SetNextItemWidth(-1);
                            { bool ch = ImGui::DragFloat("##amselw", &am.selectionWeight, 0.05f, 0.0f, 1000.0f);
                              if (ImGui::IsItemActivated()) pushUndo();
                              if (ch) { modified_ = true; updateWindowTitle(); } }

                            strField("License (SPDX id or free text)", am.license, 128);
                            strField("Provenance", am.provenance, 256);
                            strField("Source Generator / Hash", am.sourceGeneratorOrHash, 256);
                            strField("Semantic Version (this definition)", am.semanticVersion, 32);

                            // Sockets: map<string, array<float,3>> -- named
                            // anchor/socket points, key rename + vec3 edit +
                            // remove, matching the Meta editor's rename
                            // pattern (PropertiesPanel.cpp) adapted for a
                            // vec3 value instead of a string.
                            ImGui::Spacing();
                            ImGui::TextDisabled("Sockets (named local-space points)");
                            {
                                std::string renameFrom, renameTo, removeKey;
                                for (auto& [key, pos] : am.sockets) {
                                    ImGui::PushID(("sock_" + key).c_str());
                                    char keyBuf[128];
                                    std::strncpy(keyBuf, key.c_str(), sizeof(keyBuf)-1); keyBuf[127]='\0';
                                    ImGui::SetNextItemWidth(120);
                                    if (ImGui::InputText("##sockkey", keyBuf, sizeof(keyBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue) && keyBuf[0] && key != keyBuf) {
                                        renameFrom = key; renameTo = keyBuf;
                                    }
                                    ImGui::SameLine();
                                    float p[3] = { pos[0], pos[1], pos[2] };
                                    ImGui::SetNextItemWidth(-32);
                                    bool ch = ImGui::DragFloat3("##sockpos", p, 0.01f);
                                    if (ImGui::IsItemActivated()) pushUndo();
                                    if (ch) { pos = {p[0], p[1], p[2]}; modified_ = true; updateWindowTitle(); }
                                    ImGui::SameLine();
                                    if (ImGui::SmallButton("x##sockrm")) removeKey = key;
                                    ImGui::PopID();
                                }
                                if (!renameFrom.empty()) {
                                    pushUndo();
                                    auto node = am.sockets.extract(renameFrom);
                                    node.key() = renameTo;
                                    am.sockets.insert(std::move(node));
                                    modified_ = true; updateWindowTitle();
                                }
                                if (!removeKey.empty()) {
                                    pushUndo(); am.sockets.erase(removeKey);
                                    modified_ = true; updateWindowTitle();
                                }
                                if (ImGui::SmallButton("+ Add Socket")) {
                                    pushUndo();
                                    int n = 1; std::string key;
                                    do { key = "socket_" + std::to_string(n++); }
                                    while (am.sockets.count(key));
                                    am.sockets[key] = {0.f, 0.f, 0.f};
                                    modified_ = true; updateWindowTitle();
                                }
                            }

                            // LODs: map<string, string> (tier name -> def id).
                            ImGui::Spacing();
                            ImGui::TextDisabled("LOD tiers (tier name -> definition id)");
                            {
                                std::string renameFrom, renameTo, removeKey;
                                for (auto& [key, targetId] : am.lods) {
                                    ImGui::PushID(("lod_" + key).c_str());
                                    char keyBuf[64];
                                    std::strncpy(keyBuf, key.c_str(), sizeof(keyBuf)-1); keyBuf[63]='\0';
                                    ImGui::SetNextItemWidth(100);
                                    if (ImGui::InputText("##lodkey", keyBuf, sizeof(keyBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue) && keyBuf[0] && key != keyBuf) {
                                        renameFrom = key; renameTo = keyBuf;
                                    }
                                    ImGui::SameLine();
                                    char valBuf[128];
                                    std::strncpy(valBuf, targetId.c_str(), sizeof(valBuf)-1); valBuf[127]='\0';
                                    ImGui::SetNextItemWidth(-32);
                                    if (ImGui::InputText("##lodval", valBuf, sizeof(valBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                                        pushUndo(); targetId = valBuf; modified_ = true; updateWindowTitle();
                                    }
                                    ImGui::SameLine();
                                    if (ImGui::SmallButton("x##lodrm")) removeKey = key;
                                    ImGui::PopID();
                                }
                                if (!renameFrom.empty()) {
                                    pushUndo();
                                    auto node = am.lods.extract(renameFrom);
                                    node.key() = renameTo;
                                    am.lods.insert(std::move(node));
                                    modified_ = true; updateWindowTitle();
                                }
                                if (!removeKey.empty()) {
                                    pushUndo(); am.lods.erase(removeKey);
                                    modified_ = true; updateWindowTitle();
                                }
                                if (ImGui::SmallButton("+ Add LOD Tier")) {
                                    pushUndo();
                                    int n = 1; std::string key;
                                    do { key = "tier_" + std::to_string(n++); }
                                    while (am.lods.count(key));
                                    am.lods[key] = "";
                                    modified_ = true; updateWindowTitle();
                                }
                            }
                        }
                        ImGui::TreePop();
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
            // AUDIT-0055: intentionally scalars/colors only (baseColor,
            // roughness, metallic, emissive, alphaMode, doubleSided) — this
            // is the quick-access left panel. Texture-slot assignment
            // (baseColorTexture, normalTexture, metallicRoughnessTexture,
            // emissiveTexture, occlusionTexture) is only in the full
            // PropertiesPanel.cpp material editor, by design.
            if (!selectedMaterialKey_.empty() && document_.materials.count(selectedMaterialKey_)) {
                auto& mat = document_.materials[selectedMaterialKey_];
                ImGui::Separator();

                // D7: Material preview sphere
                initMatPreview();
                renderMatPreview(mat.baseColor[0], mat.baseColor[1], mat.baseColor[2],
                                 mat.roughness, mat.metallic);
                if (matPreviewTexId_) {
                    float avail = ImGui::GetContentRegionAvail().x;
                    float sz = std::min(avail, (float)kMatPreviewRes);
                    float off = (avail - sz) * 0.5f;
                    if (off > 0.f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
                    ImGui::Image((ImTextureID)(intptr_t)matPreviewTexId_, ImVec2(sz, sz));
                    ImGui::Spacing();
                }

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

                // AUDIT-0059: material-editor widgets below were missing
                // pushUndo() entirely (unlike PropertiesPanel.cpp's own copy
                // of this material editor, which already has it) --
                // IsItemActivated()-gated for the continuous-drag widgets.

                // Base color
                ImGui::TextDisabled("Base Color");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh1176 = ImGui::ColorEdit4("##matbc", mat.baseColor.data(),
                    ImGuiColorEditFlags_Float);
                    if (ImGui::IsItemActivated()) pushUndo();
                    if (_undoCh1176) {
                    modified_ = true;
                } }

                // Roughness
                ImGui::TextDisabled("Roughness");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: without it, Ctrl+Click lets a typed value go
                // out of [0,1], which would export a spec-invalid glTF
                // pbrMetallicRoughness.roughnessFactor with no other guard.
                if (ImGui::SliderFloat("##matrgh", &mat.roughness, 0.0f, 1.0f, "%.2f",
                                       ImGuiSliderFlags_AlwaysClamp)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true;
                }

                // Metallic
                ImGui::TextDisabled("Metallic");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: same out-of-[0,1]-via-Ctrl+Click risk as
                // roughness above (spec-invalid metallicFactor on export).
                if (ImGui::SliderFloat("##matmet", &mat.metallic, 0.0f, 1.0f, "%.2f",
                                       ImGuiSliderFlags_AlwaysClamp)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true;
                }

                // Emissive
                ImGui::TextDisabled("Emissive");
                ImGui::SetNextItemWidth(-1);
                { bool _undoCh1208 = ImGui::ColorEdit3("##matemi", mat.emissiveColor.data(),
                    ImGuiColorEditFlags_Float);
                    if (ImGui::IsItemActivated()) pushUndo();
                    if (_undoCh1208) {
                    modified_ = true;
                } }

                // Alpha mode
                ImGui::TextDisabled("Alpha Mode");
                const char* alphaModes[] = { "opaque", "mask", "blend" };
                int alphaIdx = 0;
                if (mat.alphaMode == "mask")  alphaIdx = 1;
                if (mat.alphaMode == "blend") alphaIdx = 2;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##matam", &alphaIdx, alphaModes, 3)) {
                    pushUndo();
                    mat.alphaMode = alphaModes[alphaIdx];
                    modified_ = true;
                }
                if (alphaIdx == 1) {
                    ImGui::TextDisabled("Alpha Cutoff");
                    ImGui::SetNextItemWidth(-1);
                    // AlwaysClamp: same out-of-[0,1]-via-Ctrl+Click risk as
                    // roughness/metallic above (spec-invalid alphaCutoff).
                    if (ImGui::SliderFloat("##matac", &mat.alphaCutoff, 0.0f, 1.0f, "%.2f",
                                           ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true;
                    }
                }

                // Double-sided
                if (ImGui::Checkbox("Double-sided##matds", &mat.doubleSided)) {
                    pushUndo();
                    modified_ = true;
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Scripts (STAB-0705, N3) — minimal: list + plain-text source
        // editor, no syntax highlighting/validation. doc.scripts already
        // parses/round-trips/exports correctly; this was the only missing
        // piece (zero editor UI existed for it before).
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Scripts")) {
            if (!selectedScriptKey_.empty() && !document_.scripts.count(selectedScriptKey_))
                selectedScriptKey_.clear();

            if (ImGui::SmallButton("+##scriptadd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "script_" + std::to_string(n++); }
                while (document_.scripts.count(key));
                Mc3::Mc3Script script;
                script.id   = key;
                script.type = "lua";
                document_.scripts[key] = script;
                selectedScriptKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add script");
            ImGui::SameLine();
            if (ImGui::SmallButton("-##scriptremove") && !selectedScriptKey_.empty()) {
                pushUndo();
                document_.scripts.erase(selectedScriptKey_);
                selectedScriptKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            ImGui::Separator();
            for (const auto& [key, script] : document_.scripts) {
                bool sel = (key == selectedScriptKey_);
                std::string label = key + "  (" + (script.type.empty() ? "lua" : script.type) + ")";
                ImGui::PushID(("script_" + key).c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedScriptKey_ = key;
                ImGui::PopID();
            }

            if (!selectedScriptKey_.empty() && document_.scripts.count(selectedScriptKey_)) {
                auto& script = document_.scripts[selectedScriptKey_];
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("ID");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##scriptid"))
                    ImGui::SetClipboardText(selectedScriptKey_.c_str());
                ImGui::TextUnformatted(selectedScriptKey_.c_str());

                ImGui::TextDisabled("Type");
                {
                    char buf[64];
                    std::strncpy(buf, script.type.c_str(), sizeof(buf)-1); buf[63]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##scripttype", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); script.type = buf; modified_ = true; updateWindowTitle();
                    }
                }

                ImGui::TextDisabled("Source (no syntax highlighting)");
                {
                    char buf[16384];
                    std::strncpy(buf, script.source.c_str(), sizeof(buf)-1); buf[16383]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputTextMultiline("##scriptsource", buf, sizeof(buf),
                            ImVec2(-1, 240), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); script.source = buf; modified_ = true; updateWindowTitle();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Audio (STAB-0706, N4) — Sounds + Music, each list+editor
        // following the same pattern as Scripts above, plus real preview
        // playback via CNA's SoundEffect/SoundEffectInstance.
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Audio")) {
            if (!audioPreviewError_.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::TextWrapped("%s", audioPreviewError_.c_str());
                ImGui::PopStyleColor();
                ImGui::Separator();
            }

            // Resolves a Mc3Sound/Mc3Music's `src` relative to the loaded
            // document's own directory, matching the same base-path
            // convention mc3togltf uses for texture URIs.
            auto resolveSrc = [&](const std::string& src) -> std::string {
                if (src.empty()) return src;
                std::filesystem::path p(src);
                return p.is_absolute() ? p.string() : (document_.sourcePath / p).string();
            };

            // --- Sounds ---
            ImGui::TextUnformatted("Sounds");
            ImGui::Separator();

            if (!selectedSoundKey_.empty() && !document_.sounds.count(selectedSoundKey_))
                selectedSoundKey_.clear();

            if (ImGui::SmallButton("+##soundadd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "sound_" + std::to_string(n++); }
                while (document_.sounds.count(key));
                Mc3::Mc3Sound sound;
                sound.id = key;
                document_.sounds[key] = sound;
                selectedSoundKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add sound");
            ImGui::SameLine();
            if (ImGui::SmallButton("-##soundremove") && !selectedSoundKey_.empty()) {
                pushUndo();
                if (audioPreviewKey_ == selectedSoundKey_) stopAudioPreview();
                document_.sounds.erase(selectedSoundKey_);
                selectedSoundKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            for (const auto& [key, sound] : document_.sounds) {
                bool sel = (key == selectedSoundKey_);
                bool playing = (key == audioPreviewKey_) && audioPreviewInstance_ &&
                               audioPreviewInstance_->getStateProperty() == Microsoft::Xna::Framework::Audio::SoundState::Playing;
                ImGui::PushID(("sound_" + key).c_str());
                if (playing) {
                    if (ImGui::SmallButton("■")) stopAudioPreview();
                } else {
                    if (ImGui::SmallButton("▶") && !sound.src.empty())
                        playAudioPreview(key, resolveSrc(sound.src), sound.loop);
                }
                ImGui::SameLine();
                if (ImGui::Selectable(key.c_str(), sel))
                    selectedSoundKey_ = key;
                ImGui::PopID();
            }

            if (!selectedSoundKey_.empty() && document_.sounds.count(selectedSoundKey_)) {
                auto& sound = document_.sounds[selectedSoundKey_];
                ImGui::Spacing();

                ImGui::TextDisabled("ID");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##soundid"))
                    ImGui::SetClipboardText(selectedSoundKey_.c_str());
                ImGui::TextUnformatted(selectedSoundKey_.c_str());

                ImGui::TextDisabled("Source path");
                {
                    char buf[512];
                    std::strncpy(buf, sound.src.c_str(), sizeof(buf)-1); buf[511]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##soundsrc", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); sound.src = buf; modified_ = true; updateWindowTitle();
                    }
                }

                if (ImGui::Checkbox("Loop##soundloop", &sound.loop)) {
                    pushUndo(); modified_ = true; updateWindowTitle();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // --- Music ---
            ImGui::TextUnformatted("Music");
            ImGui::Separator();

            if (!selectedMusicKey_.empty() && !document_.musicTracks.count(selectedMusicKey_))
                selectedMusicKey_.clear();

            if (ImGui::SmallButton("+##musicadd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "music_" + std::to_string(n++); }
                while (document_.musicTracks.count(key));
                Mc3::Mc3Music music;
                music.id = key;
                document_.musicTracks[key] = music;
                selectedMusicKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add music track");
            ImGui::SameLine();
            if (ImGui::SmallButton("-##musicremove") && !selectedMusicKey_.empty()) {
                pushUndo();
                if (audioPreviewKey_ == selectedMusicKey_) stopAudioPreview();
                document_.musicTracks.erase(selectedMusicKey_);
                selectedMusicKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            for (const auto& [key, music] : document_.musicTracks) {
                bool sel = (key == selectedMusicKey_);
                bool playing = (key == audioPreviewKey_) && audioPreviewInstance_ &&
                               audioPreviewInstance_->getStateProperty() == Microsoft::Xna::Framework::Audio::SoundState::Playing;
                ImGui::PushID(("music_" + key).c_str());
                if (playing) {
                    if (ImGui::SmallButton("■")) stopAudioPreview();
                } else {
                    if (ImGui::SmallButton("▶") && !music.src.empty())
                        playAudioPreview(key, resolveSrc(music.src), music.loop);
                }
                ImGui::SameLine();
                if (ImGui::Selectable(key.c_str(), sel))
                    selectedMusicKey_ = key;
                ImGui::PopID();
            }

            if (!selectedMusicKey_.empty() && document_.musicTracks.count(selectedMusicKey_)) {
                auto& music = document_.musicTracks[selectedMusicKey_];
                ImGui::Spacing();

                ImGui::TextDisabled("ID");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##musicid"))
                    ImGui::SetClipboardText(selectedMusicKey_.c_str());
                ImGui::TextUnformatted(selectedMusicKey_.c_str());

                ImGui::TextDisabled("Source path");
                {
                    char buf[512];
                    std::strncpy(buf, music.src.c_str(), sizeof(buf)-1); buf[511]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##musicsrc", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); music.src = buf; modified_ = true; updateWindowTitle();
                    }
                }

                if (ImGui::Checkbox("Loop##musicloop", &music.loop)) {
                    pushUndo(); modified_ = true; updateWindowTitle();
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Triggers (STAB-0707, N5) — a trigger is a named sequence of
        // steps (play-action/play-sound/run-script/play-music, each with a
        // `ref` id pointing at an entity in the corresponding collection).
        // doc.triggers already parses/round-trips/exports correctly; this
        // was the only missing piece.
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Triggers")) {
            if (!selectedTriggerKey_.empty() && !document_.triggers.count(selectedTriggerKey_))
                selectedTriggerKey_.clear();

            if (ImGui::SmallButton("+##triggeradd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "trigger_" + std::to_string(n++); }
                while (document_.triggers.count(key));
                Mc3::Mc3Trigger trigger;
                trigger.id = key;
                document_.triggers[key] = trigger;
                selectedTriggerKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add trigger");
            ImGui::SameLine();
            if (ImGui::SmallButton("-##triggerremove") && !selectedTriggerKey_.empty()) {
                pushUndo();
                document_.triggers.erase(selectedTriggerKey_);
                selectedTriggerKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            ImGui::Separator();
            for (const auto& [key, trigger] : document_.triggers) {
                bool sel = (key == selectedTriggerKey_);
                std::string label = key + "  (" + std::to_string(trigger.steps.size()) + " step"
                                   + (trigger.steps.size() == 1 ? "" : "s") + ")";
                ImGui::PushID(("trigger_" + key).c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedTriggerKey_ = key;
                ImGui::PopID();
            }

            if (!selectedTriggerKey_.empty() && document_.triggers.count(selectedTriggerKey_)) {
                auto& trigger = document_.triggers[selectedTriggerKey_];
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("ID");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##triggerid"))
                    ImGui::SetClipboardText(selectedTriggerKey_.c_str());
                ImGui::TextUnformatted(selectedTriggerKey_.c_str());

                ImGui::Spacing();
                ImGui::TextDisabled("Steps");

                static const char* kStepTypeNames[] = { "Play Action", "Play Sound", "Run Script", "Play Music" };
                int removeIdx = -1;
                for (size_t i = 0; i < trigger.steps.size(); ++i) {
                    auto& step = trigger.steps[i];
                    ImGui::PushID(static_cast<int>(i));

                    int typeIdx = static_cast<int>(step.type);
                    ImGui::SetNextItemWidth(120);
                    if (ImGui::Combo("##steptype", &typeIdx, kStepTypeNames, 4)) {
                        pushUndo();
                        step.type = static_cast<Mc3::TriggerStepType>(typeIdx);
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::SameLine();

                    char buf[256];
                    std::strncpy(buf, step.ref.c_str(), sizeof(buf)-1); buf[255]='\0';
                    ImGui::SetNextItemWidth(120);
                    if (ImGui::InputText("##stepref", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); step.ref = buf; modified_ = true; updateWindowTitle();
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x")) removeIdx = static_cast<int>(i);

                    ImGui::PopID();
                }
                if (removeIdx >= 0) {
                    pushUndo();
                    trigger.steps.erase(trigger.steps.begin() + removeIdx);
                    modified_ = true; updateWindowTitle();
                }

                if (ImGui::SmallButton("+ Add Step")) {
                    pushUndo();
                    trigger.steps.push_back(Mc3::Mc3TriggerStep{});
                    modified_ = true; updateWindowTitle();
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Embeds (STAB-0704, N2) — lowest-priority N-extension (a
        // narrow interchange feature: embedding a whole external glTF/GLB
        // asset by reference or inline base64). doc.embeds already parses/
        // round-trips/exports correctly; this was the only missing piece.
        // A Mesh object references an embed via meshSource = "embed:<id>".
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Embeds")) {
            if (!selectedEmbedKey_.empty() && !document_.embeds.count(selectedEmbedKey_))
                selectedEmbedKey_.clear();

            if (ImGui::SmallButton("+##embedadd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "embed_" + std::to_string(n++); }
                while (document_.embeds.count(key));
                Mc3::Mc3EmbedGltf embed;
                embed.id = key;
                document_.embeds[key] = embed;
                selectedEmbedKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add embed");
            ImGui::SameLine();
            if (ImGui::SmallButton("-##embedremove") && !selectedEmbedKey_.empty()) {
                pushUndo();
                document_.embeds.erase(selectedEmbedKey_);
                selectedEmbedKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            ImGui::Separator();
            for (const auto& [key, embed] : document_.embeds) {
                bool sel = (key == selectedEmbedKey_);
                std::string label = key;
                if (!embed.src.empty()) {
                    auto slash = embed.src.find_last_of("/\\");
                    label += "  " + (slash != std::string::npos ? embed.src.substr(slash+1) : embed.src);
                } else if (!embed.base64Content.empty()) {
                    label += "  (inline)";
                }
                ImGui::PushID(("embed_" + key).c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedEmbedKey_ = key;
                ImGui::PopID();
            }

            if (!selectedEmbedKey_.empty() && document_.embeds.count(selectedEmbedKey_)) {
                auto& embed = document_.embeds[selectedEmbedKey_];
                ImGui::Spacing();

                ImGui::TextDisabled("ID");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##embedid"))
                    ImGui::SetClipboardText(selectedEmbedKey_.c_str());
                ImGui::TextUnformatted(selectedEmbedKey_.c_str());
                ImGui::TextDisabled("Reference from a Mesh object's Source: embed:%s", selectedEmbedKey_.c_str());

                bool isInline = embed.isInline();
                ImGui::TextDisabled("Type");
                if (ImGui::RadioButton("External##embedtype", !isInline)) {
                    if (isInline) {
                        pushUndo(); embed.base64Content.clear(); modified_ = true; updateWindowTitle();
                    }
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Inline (base64)##embedtype", isInline)) {
                    if (!isInline) {
                        pushUndo(); embed.src.clear();
                        // isInline() requires base64Content to be non-empty
                        // (src empty && !base64Content.empty()) -- without
                        // seeding it, the radio button could never actually
                        // flip to Inline for a fresh/External entry.
                        if (embed.base64Content.empty())
                            embed.base64Content = "TODO_paste_base64_glb_data_here";
                        modified_ = true; updateWindowTitle();
                    }
                }

                if (!isInline) {
                    ImGui::TextDisabled("Source path (.glb)");
                    char buf[512];
                    std::strncpy(buf, embed.src.c_str(), sizeof(buf)-1); buf[511]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##embedsrc", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); embed.src = buf; modified_ = true; updateWindowTitle();
                    }
                } else {
                    ImGui::TextDisabled("Base64 GLB data");
                    char buf[16384];
                    std::strncpy(buf, embed.base64Content.c_str(), sizeof(buf)-1); buf[16383]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputTextMultiline("##embedbase64", buf, sizeof(buf),
                            ImVec2(-1, 120), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); embed.base64Content = buf; modified_ = true; updateWindowTitle();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: States (STAB-0708, N6) — a scene state is a named set of
        // per-object property overrides (visible/position/rotation/
        // material, each independently optional). doc.sceneStates already
        // parses/round-trips/exports correctly; this was the only missing
        // piece. `name` is treated as read-only (like every other N-
        // extension tab's id/key field this session), since it doubles as
        // the doc.sceneStates map key and renaming would need map-key-
        // rehoming logic this codebase doesn't have anywhere yet.
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("States")) {
            if (!selectedSceneStateKey_.empty() && !document_.sceneStates.count(selectedSceneStateKey_))
                selectedSceneStateKey_.clear();

            if (ImGui::SmallButton("+##stateadd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "state_" + std::to_string(n++); }
                while (document_.sceneStates.count(key));
                Mc3::Mc3SceneState state;
                state.name = key;
                document_.sceneStates[key] = state;
                selectedSceneStateKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add scene state");
            ImGui::SameLine();
            if (ImGui::SmallButton("-##stateremove") && !selectedSceneStateKey_.empty()) {
                pushUndo();
                document_.sceneStates.erase(selectedSceneStateKey_);
                selectedSceneStateKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            ImGui::Separator();
            for (const auto& [key, state] : document_.sceneStates) {
                bool sel = (key == selectedSceneStateKey_);
                std::string label = key + "  (" + std::to_string(state.overrides.size()) + " override"
                                   + (state.overrides.size() == 1 ? "" : "s") + ")";
                ImGui::PushID(("state_" + key).c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedSceneStateKey_ = key;
                ImGui::PopID();
            }

            if (!selectedSceneStateKey_.empty() && document_.sceneStates.count(selectedSceneStateKey_)) {
                auto& state = document_.sceneStates[selectedSceneStateKey_];
                ImGui::Spacing();

                ImGui::TextDisabled("Name");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##stateid"))
                    ImGui::SetClipboardText(selectedSceneStateKey_.c_str());
                ImGui::TextUnformatted(selectedSceneStateKey_.c_str());

                ImGui::Spacing();
                ImGui::TextDisabled("Object Overrides");

                int removeIdx = -1;
                for (size_t i = 0; i < state.overrides.size(); ++i) {
                    auto& ov = state.overrides[i];
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::Separator();

                    char idBuf[128];
                    std::strncpy(idBuf, ov.id.c_str(), sizeof(idBuf)-1); idBuf[127]='\0';
                    ImGui::TextDisabled("Object ID");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-40);
                    if (ImGui::InputText("##ovid", idBuf, sizeof(idBuf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); ov.id = idBuf; modified_ = true; updateWindowTitle();
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x##ovrm")) removeIdx = static_cast<int>(i);

                    // Visible (optional<bool>)
                    {
                        bool has = ov.visible.has_value();
                        if (ImGui::Checkbox("Override Visible##ovvis", &has)) {
                            pushUndo();
                            ov.visible = has ? std::optional<bool>(true) : std::nullopt;
                            modified_ = true; updateWindowTitle();
                        }
                        if (ov.visible.has_value()) {
                            ImGui::SameLine();
                            bool v = *ov.visible;
                            if (ImGui::Checkbox("Value##ovvisval", &v)) {
                                pushUndo(); ov.visible = v; modified_ = true; updateWindowTitle();
                            }
                        }
                    }

                    // Position (optional<array<float,3>>)
                    {
                        bool has = ov.position.has_value();
                        if (ImGui::Checkbox("Override Position##ovpos", &has)) {
                            pushUndo();
                            ov.position = has ? std::optional<std::array<float,3>>({0,0,0}) : std::nullopt;
                            modified_ = true; updateWindowTitle();
                        }
                        if (ov.position.has_value()) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(180);
                            { bool _undoCh1792 = ImGui::DragFloat3("##ovposval", ov.position->data(), 0.01f);
                            if (ImGui::IsItemActivated()) pushUndo();
                            if (_undoCh1792) {
                                modified_ = true; updateWindowTitle();
                            } }
                        }
                    }

                    // Rotation (optional<array<float,3>>)
                    {
                        bool has = ov.rotation.has_value();
                        if (ImGui::Checkbox("Override Rotation##ovrot", &has)) {
                            pushUndo();
                            ov.rotation = has ? std::optional<std::array<float,3>>({0,0,0}) : std::nullopt;
                            modified_ = true; updateWindowTitle();
                        }
                        if (ov.rotation.has_value()) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(180);
                            { bool _undoCh1810 = ImGui::DragFloat3("##ovrotval", ov.rotation->data(), 0.5f);
                            if (ImGui::IsItemActivated()) pushUndo();
                            if (_undoCh1810) {
                                modified_ = true; updateWindowTitle();
                            } }
                        }
                    }

                    // Material (optional<string>)
                    {
                        bool has = ov.material.has_value();
                        if (ImGui::Checkbox("Override Material##ovmat", &has)) {
                            pushUndo();
                            ov.material = has ? std::optional<std::string>("") : std::nullopt;
                            modified_ = true; updateWindowTitle();
                        }
                        if (ov.material.has_value()) {
                            ImGui::SameLine();
                            char matBuf[128];
                            std::strncpy(matBuf, ov.material->c_str(), sizeof(matBuf)-1); matBuf[127]='\0';
                            ImGui::SetNextItemWidth(150);
                            if (ImGui::InputText("##ovmatval", matBuf, sizeof(matBuf),
                                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                                pushUndo(); ov.material = std::string(matBuf); modified_ = true; updateWindowTitle();
                            }
                        }
                    }

                    ImGui::PopID();
                }
                if (removeIdx >= 0) {
                    pushUndo();
                    state.overrides.erase(state.overrides.begin() + removeIdx);
                    modified_ = true; updateWindowTitle();
                }

                ImGui::Separator();
                if (ImGui::SmallButton("+ Add Override")) {
                    pushUndo();
                    state.overrides.push_back(Mc3::Mc3ObjectOverride{});
                    modified_ = true; updateWindowTitle();
                }
            }

            ImGui::EndTabItem();
        }

        // ---------------------------------------------------------------
        // Tab: Imports (R101/SYS-W14-13) -- doc.imports (std::vector<Mc3Import>,
        // an ordered list rather than a map like scripts/triggers/sounds, so no
        // "selected key" concept: just a direct index-based row editor,
        // mirroring the Triggers tab's per-step editor above.
        // ---------------------------------------------------------------
        if (ImGui::BeginTabItem("Imports")) {
            ImGui::TextDisabled("Pulls a .mc3lib library into this document under a");
            ImGui::TextDisabled("local alias, so instances can reference its definitions");
            ImGui::TextDisabled("as \"<namespace>:<definitionId>\".");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            int removeIdx = -1;
            for (size_t i = 0; i < document_.imports.size(); ++i) {
                auto& imp = document_.imports[i];
                ImGui::PushID(static_cast<int>(i));

                ImGui::TextDisabled("Namespace");
                {
                    char buf[128];
                    std::strncpy(buf, imp.importNamespace.c_str(), sizeof(buf)-1); buf[127]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##impns", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); imp.importNamespace = buf; modified_ = true; updateWindowTitle();
                    }
                }
                ImGui::TextDisabled("Source (mc3lib://<library-name>@<version>)");
                {
                    char buf[256];
                    std::strncpy(buf, imp.source.c_str(), sizeof(buf)-1); buf[255]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##impsrc", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); imp.source = buf; modified_ = true; updateWindowTitle();
                    }
                }
                ImGui::TextDisabled("Hash (optional, \"sha256:...\" -- empty = not pinned)");
                {
                    char buf[128];
                    std::strncpy(buf, imp.hash.c_str(), sizeof(buf)-1); buf[127]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##imphash", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); imp.hash = buf; modified_ = true; updateWindowTitle();
                    }
                }
                if (ImGui::SmallButton("Remove")) removeIdx = static_cast<int>(i);

                ImGui::PopID();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
            if (removeIdx >= 0) {
                pushUndo();
                document_.imports.erase(document_.imports.begin() + removeIdx);
                modified_ = true; updateWindowTitle();
            }

            if (ImGui::SmallButton("+ Add Import")) {
                pushUndo();
                document_.imports.push_back(Mc3::Mc3Import{});
                modified_ = true; updateWindowTitle();
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
